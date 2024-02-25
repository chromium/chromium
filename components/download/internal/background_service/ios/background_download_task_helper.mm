// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_task_helper.h"

#import <Foundation/Foundation.h>

#include <deque>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/features.h"
#include "net/base/apple/url_conversions.h"

namespace {
bool g_ignore_localhost_ssl_error_for_testing = false;
}

using AuthenticationChallengeBlock =
    void (^)(NSURLSessionAuthChallengeDisposition disposition,
             NSURLCredential* credential);
using CompletionCallback =
    download::BackgroundDownloadTaskHelper::CompletionCallback;
using UpdateCallback = download::BackgroundDownloadTaskHelper::UpdateCallback;

class DownloadTaskInfo {
 public:
  DownloadTaskInfo(const base::FilePath& download_path,
                   CompletionCallback completion_callback,
                   UpdateCallback update_callback)
      : download_path_(download_path),
        completion_callback_(std::move(completion_callback)),
        update_callback_(update_callback) {}
  ~DownloadTaskInfo() = default;

  base::FilePath download_path_;
  CompletionCallback completion_callback_;
  UpdateCallback update_callback_;
};

@interface BackgroundDownloadDelegate
    : NSObject <NSURLSessionDownloadDelegate> {
 @private
  // Callback to invoke once background session completes.
  base::OnceClosure _sessionCompletionHandler;
}

- (instancetype)initWithTaskRunner:
    (scoped_refptr<base::SingleThreadTaskRunner>)taskRunner;

- (void)setSessionCompletionHandler:(base::OnceClosure)sessionCompletionHandler;
@end

@implementation BackgroundDownloadDelegate {
  std::map<NSURLSessionDownloadTask*, std::unique_ptr<DownloadTaskInfo>>
      _downloadTaskInfos;
  scoped_refptr<base::SingleThreadTaskRunner> _taskRunner;
}

- (instancetype)initWithTaskRunner:
    (scoped_refptr<base::SingleThreadTaskRunner>)taskRunner {
  _taskRunner = taskRunner;
  return self;
}

- (void)addDownloadTask:(NSURLSessionDownloadTask*)downloadTask
       downloadTaskInfo:(std::unique_ptr<DownloadTaskInfo>)downloadTaskInfo {
  _downloadTaskInfos[downloadTask] = std::move(downloadTaskInfo);
}

- (void)onDownloadCompletion:(bool)success
                downloadTask:(NSURLSessionDownloadTask*)downloadTask
                    fileSize:(int64_t)fileSize {
  std::unique_ptr<DownloadTaskInfo> taskInfo;
  // Remove the download from map if it exists.
  auto it = _downloadTaskInfos.find(downloadTask);
  if (it != _downloadTaskInfos.end()) {
    taskInfo = std::move(it->second);
    _downloadTaskInfos.erase(it);
  }

  base::FilePath filePath;
  if (taskInfo) {
    filePath = taskInfo->download_path_;
  }

  if (taskInfo && taskInfo->completion_callback_) {
    // Invoke the completion callback on main thread.
    _taskRunner->PostTask(
        FROM_HERE, base::BindOnce(std::move(taskInfo->completion_callback_),
                                  success, filePath, fileSize));
  }
}

- (void)setSessionCompletionHandler:
    (base::OnceClosure)sessionCompletionHandler {
  _sessionCompletionHandler = std::move(sessionCompletionHandler);
}

#pragma mark - NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession*)session
          downloadTask:(NSURLSessionDownloadTask*)downloadTask
     didResumeAtOffset:(int64_t)fileOffset
    expectedTotalBytes:(int64_t)expectedTotalBytes {
  DVLOG(1) << __func__ << " , offset:" << fileOffset
           << " , expectedTotalBytes:" << expectedTotalBytes;
  NOTIMPLEMENTED();
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
  DVLOG(1) << __func__ << ",byte written: " << bytesWritten
           << ", totalBytesWritten:" << totalBytesWritten
           << ", totalBytesExpectedToWrite:" << totalBytesExpectedToWrite;
  auto it = _downloadTaskInfos.find(downloadTask);
  if (it != _downloadTaskInfos.end() && it->second->update_callback_) {
    _taskRunner->PostTask(
        FROM_HERE,
        base::BindRepeating(it->second->update_callback_, totalBytesWritten));
  }
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  DVLOG(1) << __func__;
  if (!location) {
    [self onDownloadCompletion:/*success=*/false
                  downloadTask:downloadTask
                      fileSize:0];
    return;
  }

  // Analyze the response code. Treat non http 200 as failure downloads.
  NSURLResponse* response = [downloadTask response];
  if (response && [response isKindOfClass:[NSHTTPURLResponse class]]) {
    NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
    if ([httpResponse statusCode] != 200) {
      [self onDownloadCompletion:/*success=*/false
                    downloadTask:downloadTask
                        fileSize:0];
      return;
    }
  }

  auto it = _downloadTaskInfos.find(downloadTask);
  if (it == _downloadTaskInfos.end()) {
    LOG(ERROR) << "Failed to find the download task.";
    [self onDownloadCompletion:/*success=*/false
                  downloadTask:downloadTask
                      fileSize:0];
    return;
  }

  // Move the downloaded file from platform temporary directory to download
  // service's target directory. This must happen immediately on the current
  // thread or iOS may delete the file.
  const base::FilePath tempPath =
      base::apple::NSStringToFilePath([location path]);
  if (!base::Move(tempPath, it->second->download_path_)) {
    LOG(ERROR) << "Failed to move file from:" << tempPath
               << ", to:" << it->second->download_path_;
    [self onDownloadCompletion:/*success=*/false
                  downloadTask:downloadTask
                      fileSize:0];
    return;
  }

  // Get the file size on current thread.
  int64_t fileSize = 0;
  if (!base::GetFileSize(it->second->download_path_, &fileSize)) {
    LOG(ERROR) << "Failed to get file size from:" << it->second->download_path_;
    [self onDownloadCompletion:/*success=*/false
                  downloadTask:downloadTask
                      fileSize:0];
    return;
  }
  [self onDownloadCompletion:/*success=*/true
                downloadTask:downloadTask
                    fileSize:fileSize];
}

- (void)URLSessionDidFinishEventsForBackgroundURLSession:
    (NSURLSession*)session {
  if (!_sessionCompletionHandler.is_null()) {
    // Nothing should be called after invoking completionHandler.
    std::move(_sessionCompletionHandler).Run();
  }
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  VLOG(1) << __func__;

  if (!error)
    return;

  NSURLSessionDownloadTask* downloadTask =
      [task isKindOfClass:[NSURLSessionDownloadTask class]]
          ? (NSURLSessionDownloadTask*)task
          : nil;
  if (!downloadTask) {
    LOG(ERROR) << "Encountered errors unrelated to download.";
    return;
  }

  [self onDownloadCompletion:/*success=*/false
                downloadTask:downloadTask
                    fileSize:0];
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(AuthenticationChallengeBlock)completionHandler {
  DCHECK(completionHandler);
  if ([challenge.protectionSpace.authenticationMethod
          isEqualToString:NSURLAuthenticationMethodServerTrust]) {
    if (g_ignore_localhost_ssl_error_for_testing &&
        [challenge.protectionSpace.host isEqualToString:@"127.0.0.1"]) {
      NSURLCredential* credential = [NSURLCredential
          credentialForTrust:challenge.protectionSpace.serverTrust];
      completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
      return;
    }
  }
  completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}
@end

namespace download {

// Object for passing the delegate and session to the UI thread as unique_ptr
// doesn't work on both of those.
struct URLSessionHelper {
  URLSessionHelper(BackgroundDownloadDelegate* delegate, NSURLSession* session)
      : delegate(delegate), session(session) {}
  BackgroundDownloadDelegate* delegate = nullptr;
  NSURLSession* session = nullptr;
};

using CreateUrlSessionCallback =
    base::OnceCallback<void(std::unique_ptr<URLSessionHelper>)>;

void CreateNSURLSession(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        CreateUrlSessionCallback callback) {
  const int kIdentifierSuffix = 1000000;
  std::string identifier =
      base::StringPrintf("%s-%d", download::kBackgroundDownloadIdentifierPrefix,
                         base::RandInt(0, kIdentifierSuffix));
  NSURLSessionConfiguration* configuration =
      base::FeatureList::IsEnabled(
          download::kDownloadServiceForegroundSessionIOSFeature)
          ? [NSURLSessionConfiguration defaultSessionConfiguration]
          : [NSURLSessionConfiguration
                backgroundSessionConfigurationWithIdentifier:
                    base::SysUTF8ToNSString(identifier)];
  configuration.sessionSendsLaunchEvents = YES;
  // TODO(qinmin): Check if we need 2 sessions here, since discretionary
  // value may be different.
  configuration.discretionary = true;
  BackgroundDownloadDelegate* delegate =
      [[BackgroundDownloadDelegate alloc] initWithTaskRunner:task_runner];
  NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration
                                                        delegate:delegate
                                                   delegateQueue:nil];
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::make_unique<URLSessionHelper>(delegate, session)));
}

// Implementation of BackgroundDownloadTaskHelper based on
// NSURLSessionDownloadTask api.
// This class lives on main thread and all the callbacks will be invoked on main
// thread. The NSURLSessionDownloadDelegate it uses will broadcast download
// events on a background thread.
class BackgroundDownloadTaskHelperImpl : public BackgroundDownloadTaskHelper {
 public:
  BackgroundDownloadTaskHelperImpl() = default;
  ~BackgroundDownloadTaskHelperImpl() override {
    delegate_ = nullptr;
    [session_ invalidateAndCancel];
  }

 private:
  struct DownloadTask {
    DownloadTask(const std::string& guid,
                 const base::FilePath& target_path,
                 const RequestParams& request_params,
                 CompletionCallback completion_callback,
                 UpdateCallback update_callback)
        : guid(guid),
          target_path(target_path),
          request_params(request_params),
          completion_callback(std::move(completion_callback)),
          update_callback(update_callback) {}

    std::string guid;
    base::FilePath target_path;
    RequestParams request_params;
    CompletionCallback completion_callback;
    UpdateCallback update_callback;
  };

  void OnNSURLSessionCreated(std::unique_ptr<URLSessionHelper> session_helper) {
    delegate_ = session_helper->delegate;
    session_ = session_helper->session;
    ProcessDownloadTasks();
  }

  void StartDownload(const std::string& guid,
                     const base::FilePath& target_path,
                     const RequestParams& request_params,
                     const SchedulingParams& scheduling_params,
                     CompletionCallback completion_callback,
                     UpdateCallback update_callback) override {
    DCHECK(!guid.empty());
    DCHECK(!target_path.empty());
    download_tasks_.emplace_back(guid, target_path, request_params,
                                 std::move(completion_callback),
                                 update_callback);
    // Initialize the NSURLSession and delegate on another thread due to
    // http://crbug.com/1359437.
    if (!is_initializing_) {
      is_initializing_ = true;
      CreateUrlSessionCallback cb = base::BindOnce(
          &BackgroundDownloadTaskHelperImpl::OnNSURLSessionCreated,
          weak_ptr_factory_.GetWeakPtr());
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&CreateNSURLSession,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(cb)));
      return;
    }

    if (delegate_)
      ProcessDownloadTasks();
  }

  void ProcessDownloadTasks() {
    while (!download_tasks_.empty()) {
      ProcessDownloadTask(download_tasks_.front());
      download_tasks_.pop_front();
    }
  }

  void ProcessDownloadTask(DownloadTask& task) {
    NSURL* url = net::NSURLWithGURL(task.request_params.url);
    NSMutableURLRequest* request =
        [[NSMutableURLRequest alloc] initWithURL:url];
    [request setHTTPMethod:base::SysUTF8ToNSString(task.request_params.method)];
    net::HttpRequestHeaders::Iterator it(task.request_params.request_headers);
    while (it.GetNext()) {
      [request setValue:base::SysUTF8ToNSString(it.value())
          forHTTPHeaderField:base::SysUTF8ToNSString(it.name())];
    }

    NSURLSessionDownloadTask* downloadTask =
        [session_ downloadTaskWithRequest:request];
    auto download_task_info = std::make_unique<DownloadTaskInfo>(
        task.target_path, std::move(task.completion_callback),
        task.update_callback);
    [delegate_ addDownloadTask:downloadTask
              downloadTaskInfo:std::move(download_task_info)];
    [downloadTask resume];
  }

  void HandleEventsForBackgroundURLSession(
      base::OnceClosure completion_handler) override {
    delegate_.sessionCompletionHandler = std::move(completion_handler);
  }

  BackgroundDownloadDelegate* delegate_ = nullptr;
  NSURLSession* session_ = nullptr;
  std::deque<DownloadTask> download_tasks_;
  bool is_initializing_ = false;

  base::WeakPtrFactory<BackgroundDownloadTaskHelperImpl> weak_ptr_factory_{
      this};
};

// static
std::unique_ptr<BackgroundDownloadTaskHelper>
BackgroundDownloadTaskHelper::Create() {
  return std::make_unique<BackgroundDownloadTaskHelperImpl>();
}

// static
void BackgroundDownloadTaskHelper::SetIgnoreLocalSSLErrorForTesting(
    bool ignore) {
  g_ignore_localhost_ssl_error_for_testing = ignore;
}

}  // namespace download
