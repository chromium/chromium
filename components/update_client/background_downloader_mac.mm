// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/update_client/background_downloader_mac.h"

#import <Foundation/Foundation.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#import "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#import "components/update_client/background_downloader_mac_delegate.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "url/gurl.h"

namespace {

// Callback invoked by DownloadDelegate when a download has finished.
using DelegateDownloadCompleteCallback = base::RepeatingCallback<
    void(const GURL&, const base::FilePath&, int, int64_t, int64_t)>;

// Callback invoked by DownloadDelegate when download metrics are available.
using DelegateMetricsCollectedCallback =
    base::RepeatingCallback<void(const GURL& url, uint64_t download_time_ms)>;

// Callback invoked by DownloadDelegate when progress has been made on a task.
using DelegateDownloadProgressCallback =
    base::RepeatingCallback<void(const GURL&)>;
using OnDownloadCompleteCallback = update_client::
    BackgroundDownloaderSharedSession::OnDownloadCompleteCallback;

// The age at which unclaimed downloads should be evicted from the cache.
constexpr base::TimeDelta kMaxCachedDownloadAge = base::Days(2);

// How often to perform periodic actions on download tasks.
constexpr base::TimeDelta kTaskPollingInterval = base::Minutes(5);

// The maximum number of tasks the downloader can have active at once.
constexpr int kMaxTasks = 10;

// How long to tolerate a background task that has not made any progress.
constexpr base::TimeDelta kNoProgressTimeout = base::Minutes(15);

// The maximum duration a task can exist before giving up.
constexpr base::TimeDelta kMaxTaskAge = base::Days(3);

// These methods have been copied from //net/base/apple/url_conversions.h to
// avoid introducing a dependancy on //net.
NSURL* NSURLWithGURL(const GURL& url) {
  if (!url.is_valid()) {
    return nil;
  }

  // NSURL strictly enforces RFC 1738 which requires that certain characters
  // are always encoded. These characters are: "<", ">", """, "#", "%", "{",
  // "}", "|", "\", "^", "~", "[", "]", and "`".
  //
  // GURL leaves some of these characters unencoded in the path, query, and
  // ref. This function manually encodes those components, and then passes the
  // result to NSURL.
  GURL::Replacements replacements;
  std::string escaped_path = base::EscapeNSURLPrecursor(url.path());
  std::string escaped_query = base::EscapeNSURLPrecursor(url.query());
  std::string escaped_ref = base::EscapeNSURLPrecursor(url.ref());
  if (!escaped_path.empty()) {
    replacements.SetPathStr(escaped_path);
  }
  if (!escaped_query.empty()) {
    replacements.SetQueryStr(escaped_query);
  }
  if (!escaped_ref.empty()) {
    replacements.SetRefStr(escaped_ref);
  }
  GURL escaped_url = url.ReplaceComponents(replacements);

  NSString* escaped_url_string =
      [NSString stringWithUTF8String:escaped_url.spec().c_str()];
  return [NSURL URLWithString:escaped_url_string];
}

}  // namespace

namespace update_client {

class BackgroundDownloaderSharedSessionImpl {
 public:
  BackgroundDownloaderSharedSessionImpl(const base::FilePath& download_cache,
                                        const std::string& session_identifier)
      : download_cache_(download_cache) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    UpdateClientDownloadDelegate* delegate = [[UpdateClientDownloadDelegate
        alloc]
           initWithDownloadCache:download_cache_
        downloadCompleteCallback:
            base::BindRepeating(
                [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl>
                       weak_this,
                   const GURL& url, const base::FilePath& location, int error,
                   int64_t downloaded_bytes, int64_t total_bytes) {
                  if (weak_this) {
                    weak_this->OnDownloadComplete(
                        url, location, error, downloaded_bytes, total_bytes);
                  }
                },
                weak_factory_.GetWeakPtr())
        metricsCollectedCallback:
            base::BindRepeating(
                [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl>
                       weak_this,
                   const GURL& url, uint64_t download_time_ms) {
                  if (weak_this) {
                    weak_this->OnMetricsCollected(url, download_time_ms);
                  }
                },
                weak_factory_.GetWeakPtr())
                progressCallback:
                    base::BindRepeating(
                        [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl>
                               weak_this,
                           const GURL& url) {
                          if (weak_this) {
                            weak_this->OnDownloadProgressMade(url);
                          }
                        },
                        weak_factory_.GetWeakPtr())];

    NSURLSessionConfiguration* config = [NSURLSessionConfiguration
        backgroundSessionConfigurationWithIdentifier:base::SysUTF8ToNSString(
                                                         session_identifier)];
    config.timeoutIntervalForResource = kMaxTaskAge.InSeconds();
    session_ = [NSURLSession sessionWithConfiguration:config
                                             delegate:delegate
                                        delegateQueue:nil];

    periodic_task_timer_.Start(
        FROM_HERE, kTaskPollingInterval,
        base::BindRepeating(
            [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl> weak_this) {
              if (weak_this) {
                weak_this->StartPeriodicTasks();
              }
            },
            weak_factory_.GetWeakPtr()));
  }

  ~BackgroundDownloaderSharedSessionImpl() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void DoStartDownload(const GURL& url, OnDownloadCompleteCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!session_) {
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.error =
          static_cast<int>(CrxDownloaderError::MAC_BG_SESSION_INVALIDATED);
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kImmediateError);
      callback.Run(false,
                   {metrics.error, metrics.extra_code1, base::FilePath()},
                   metrics);
      return;
    }

    if (downloads_.contains(url)) {
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.error =
          static_cast<int>(CrxDownloaderError::MAC_BG_DUPLICATE_DOWNLOAD);
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kImmediateError);
      callback.Run(false,
                   {metrics.error, metrics.extra_code1, base::FilePath()},
                   metrics);
      return;
    }

    if (HandleDownloadFromCache(url, callback)) {
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kDownloadRecoveredFromCache);
      return;
    }

    QueryOngoingDownloads(
        url, base::BindOnce(
                 [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl> impl,
                    const GURL& url, OnDownloadCompleteCallback callback,
                    bool has_download, int num_tasks) {
                   if (impl) {
                     impl->OnDownloadsQueried(url, std::move(callback),
                                              has_download, num_tasks);
                   }
                 },
                 weak_factory_.GetWeakPtr(), url, std::move(callback)));
  }

  void OnDownloadsQueried(const GURL& url,
                          OnDownloadCompleteCallback callback,
                          bool has_download,
                          int num_tasks) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (has_download) {
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kSessionHasOngoingDownload);
      downloads_.emplace(url, callback);
    } else if (num_tasks >= kMaxTasks) {
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.error =
          static_cast<int>(CrxDownloaderError::MAC_BG_SESSION_TOO_MANY_TASKS);
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kTooManyTasks);
      callback.Run(false,
                   {metrics.error, metrics.extra_code1, base::FilePath()},
                   metrics);
    } else {
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kNewDownloadTaskCreated);
      NSMutableURLRequest* urlRequest =
          [[NSMutableURLRequest alloc] initWithURL:NSURLWithGURL(url)];
      NSURLSessionDownloadTask* downloadTask =
          [session_ downloadTaskWithRequest:urlRequest];
      downloadTask.priority = NSURLSessionTaskPriorityHigh;

      [downloadTask resume];
      downloads_.emplace(url, callback);
    }
  }

  void InvalidateAndCancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (session_) {
      [session_ invalidateAndCancel];
      session_ = nullptr;
    }
  }

 private:
  struct DownloadResult {
    DownloadResult(bool is_handled,
                   const CrxDownloader::Result& result,
                   const CrxDownloader::DownloadMetrics& download_metrics)
        : is_handled(is_handled),
          result(result),
          download_metrics(download_metrics) {}

    explicit DownloadResult(uint64_t download_time_ms) {
      download_metrics.download_time_ms = download_time_ms;
    }

    bool is_handled = false;
    CrxDownloader::Result result;
    CrxDownloader::DownloadMetrics download_metrics;
  };

  // Looks for a completed download in the cache. Returns false if the cache
  // does not contain `url`.
  bool HandleDownloadFromCache(const GURL& url,
                               OnDownloadCompleteCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::FilePath cached_path = download_cache_.Append(URLToFilename(url));
    if (!base::PathExists(cached_path)) {
      return false;
    }

    if (results_.contains(url)) {
      // The download was completed by this
      // BackgroundDownloaderSharedSessionImpl, thus the metrics are available.
      DownloadResult result = results_.at(url);
      callback.Run(result.is_handled, result.result, result.download_metrics);
    } else {
      int64_t download_size = -1;
      if (!base::GetFileSize(cached_path, &download_size)) {
        LOG(ERROR) << "Failed determine file size for " << cached_path;
      }
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.downloaded_bytes = download_size;
      metrics.total_bytes = download_size;
      callback.Run(true,
                   {static_cast<int>(CrxDownloaderError::NONE),
                    /*extra_code1=*/0, cached_path},
                   metrics);
    }

    return true;
  }

  // Queries the tasks owned by the background session to determine if
  // an existing download exists for a URL and how many jobs are ongoing.
  void QueryOngoingDownloads(
      const GURL& url,
      base::OnceCallback<void(bool hasDownload, int numTasks)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(session_);
    // A copy of the URL is needed so that a reference is not captured by the
    // block.
    GURL url_for_block(url);
    scoped_refptr<base::SequencedTaskRunner> reply_sequence =
        base::SequencedTaskRunner::GetCurrentDefault();
    __block base::OnceCallback<void(bool, int)> block_scoped_callback =
        std::move(callback);
    [session_ getAllTasksWithCompletionHandler:^(
                  NSArray<__kindof NSURLSessionTask*>* _Nonnull tasks) {
      bool has_url = false;
      for (NSURLSessionTask* task in tasks) {
        if (url_for_block == GURLWithNSURL([task originalRequest].URL)) {
          // It has been observed that download tasks which have been
          // reassociated with this process via the recreation of a NSURLSession
          // with a background identifier report a state of
          // NSURLSessionTaskStateRunning but do not make progress.
          // Interestingly, calling resume on these tasks (which is documented
          // as having no effect on running tasks) seems to get things moving
          // again.
          [task resume];
          has_url = true;
          break;
        }
      }
      reply_sequence->PostTask(
          FROM_HERE, base::BindOnce(std::move(block_scoped_callback), has_url,
                                    tasks.count));
    }];
  }

  // Called by the delegate when the download has completed.
  void OnDownloadComplete(const GURL& url,
                          const base::FilePath& location,
                          int error,
                          int64_t downloaded_bytes,
                          int64_t total_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool had_result = results_.contains(url);
    bool is_handled = error == 0 || (error >= 500 && error < 600);
    CrxDownloader::Result result = {error, /*extra_code1=*/0, location};
    CrxDownloader::DownloadMetrics download_metrics =
        had_result ? results_.at(url).download_metrics
                   : CrxDownloader::DownloadMetrics{};
    download_metrics.url = url;
    download_metrics.downloader =
        update_client::CrxDownloader::DownloadMetrics::kBackgroundMac;
    download_metrics.error = error;
    download_metrics.downloaded_bytes = downloaded_bytes;
    download_metrics.total_bytes = total_bytes;
    results_.insert_or_assign(
        url, DownloadResult(is_handled, result, download_metrics));

    if (had_result) {
      OnDownloadResultReady(url);
    }
  }

  // Called by the delegate when the download has completed.
  void OnMetricsCollected(const GURL& url, uint64_t download_time_ms) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool had_result = results_.contains(url);
    DownloadResult result =
        had_result ? results_.at(url) : DownloadResult(download_time_ms);
    result.download_metrics.download_time_ms = download_time_ms;
    results_.insert_or_assign(url, std::move(result));

    if (had_result) {
      OnDownloadResultReady(url);
    }
  }

  // Called when both completion and metrics have been recorded for a download.
  // If the download was specifically requested via `DoStartDownload`,
  // completion is signaled to the caller. Otherwise, the result is stored until
  // the download is retrieved from cache.
  void OnDownloadResultReady(const GURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(results_.contains(url));

    bool requestor_known = downloads_.contains(url);
    metrics::RecordBDMResultRequestorKnown(requestor_known);
    if (requestor_known) {
      DownloadResult result = results_.at(url);
      downloads_.at(url).Run(result.is_handled, result.result,
                             result.download_metrics);
      results_.erase(url);
      downloads_.erase(url);
      if (last_progress_times_.contains(url)) {
        last_progress_times_.erase(url);
      }
    }
  }

  // Called when the delegate has noticed progress being made on a download.
  void OnDownloadProgressMade(const GURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    last_progress_times_.insert_or_assign(url, base::Time::Now());
  }

  void StartPeriodicTasks() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Clean the download cache of stale files.
    base::FileEnumerator(download_cache_, false, base::FileEnumerator::FILES)
        .ForEach([](const base::FilePath& download) {
          base::File::Info info;
          if (base::GetFileInfo(download, &info) &&
              base::Time::Now() - info.creation_time > kMaxCachedDownloadAge) {
            base::DeleteFile(download);
          }
        });

    if (session_) {
      __block base::OnceCallback<void(
          NSArray<__kindof NSURLSessionTask*>* _Nonnull)>
          callback = base::BindOnce(
              [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl> weak_this,
                 NSArray<__kindof NSURLSessionTask*>* _Nonnull tasks) {
                if (weak_this) {
                  weak_this->CompletePeriodicTasks(tasks);
                }
              },
              weak_factory_.GetWeakPtr());
      scoped_refptr<base::SequencedTaskRunner> reply_sequence =
          base::SequencedTaskRunner::GetCurrentDefault();
      [session_ getAllTasksWithCompletionHandler:^(
                    NSArray<__kindof NSURLSessionTask*>* _Nonnull tasks) {
        reply_sequence->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), tasks));
      }];
    }
  }

  void CompletePeriodicTasks(
      NSArray<__kindof NSURLSessionTask*>* _Nonnull tasks) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!session_) {
      return;
    }

    base::flat_map<GURL, base::Time> filtered_progresses;
    const base::Time now = base::Time::Now();
    for (NSURLSessionTask* task in tasks) {
      if (task.state != NSURLSessionTaskState::NSURLSessionTaskStateRunning) {
        continue;
      }

      const GURL url = GURLWithNSURL([task originalRequest].URL);
      if (!last_progress_times_.contains(url)) {
        // If the last progress time is unknown it should be set to now so that
        // the task can be cleaned even if it fails to send a progress update.
        filtered_progresses.emplace(url, now);
      } else {
        const base::Time& last_progress_time = last_progress_times_.at(url);
        if (now - last_progress_time > kNoProgressTimeout) {
          [task cancel];
        } else {
          filtered_progresses.emplace(url, last_progress_time);
        }
      }
    }

    last_progress_times_ = std::move(filtered_progresses);
  }

  // Returns a `CrxDownloader::DownloadMetrics` with url and downloader set.
  static CrxDownloader::DownloadMetrics GetDefaultMetrics(const GURL& url) {
    CrxDownloader::DownloadMetrics metrics;
    metrics.url = url;
    metrics.downloader =
        CrxDownloader::DownloadMetrics::Downloader::kBackgroundMac;
    metrics.error = 0;
    metrics.extra_code1 = 0;
    return metrics;
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const base::FilePath download_cache_;
  NSURLSession* session_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the (possibly partial) results of a download.
  base::flat_map<GURL, DownloadResult> results_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the last time progress was recorded for a download.
  base::flat_map<GURL, base::Time> last_progress_times_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks which downloads have been requested. This is used to notify the
  // CrxDownloader only of the downloads it has requested in the lifetime of
  // this BackgroundDownloaderSharedSessionImpl, as opposed to downloads which
  // were started by a previous BackgroundDownloaderSharedSessionImpl.
  base::flat_map<GURL, OnDownloadCompleteCallback> downloads_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingTimer periodic_task_timer_;
  base::WeakPtrFactory<BackgroundDownloaderSharedSessionImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

BackgroundDownloader::BackgroundDownloader(
    scoped_refptr<CrxDownloader> successor,
    scoped_refptr<BackgroundDownloaderSharedSession> impl,
    scoped_refptr<base::SequencedTaskRunner> background_sequence_)
    : CrxDownloader(std::move(successor)),
      shared_session_(impl),
      background_sequence_(background_sequence_) {}

BackgroundDownloader::~BackgroundDownloader() = default;

base::OnceClosure BackgroundDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DoStartDownload(
      url, base::BindPostTaskToCurrentDefault(
               base::BindRepeating(&BackgroundDownloader::OnDownloadComplete,
                                   base::WrapRefCounted(this)),
               FROM_HERE));
}

base::OnceClosure BackgroundDownloader::DoStartDownload(
    const GURL& url,
    OnDownloadCompleteCallback on_download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundDownloaderSharedSession::DoStartDownload,
                     shared_session_, url, on_download_complete_callback));
  return base::DoNothing();
}

// BackgroundDownloaderSharedSessionProxy manages an implementation bound to a
// background sequence.
class BackgroundDownloaderSharedSessionProxy
    : public BackgroundDownloaderSharedSession {
 public:
  BackgroundDownloaderSharedSessionProxy(
      scoped_refptr<base::SequencedTaskRunner> background_sequence,
      const base::FilePath& download_cache,
      const std::string& session_identifier)
      : impl_(background_sequence, download_cache, session_identifier) {}

  void DoStartDownload(
      const GURL& url,
      OnDownloadCompleteCallback on_download_complete_callback) override {
    impl_.AsyncCall(&BackgroundDownloaderSharedSessionImpl::DoStartDownload)
        .WithArgs(url, std::move(on_download_complete_callback));
  }

  void InvalidateAndCancel() override {
    impl_.AsyncCall(
        &BackgroundDownloaderSharedSessionImpl::InvalidateAndCancel);
  }

 private:
  ~BackgroundDownloaderSharedSessionProxy() override = default;

  base::SequenceBound<BackgroundDownloaderSharedSessionImpl> impl_;
};

scoped_refptr<BackgroundDownloaderSharedSession>
MakeBackgroundDownloaderSharedSession(
    scoped_refptr<base::SequencedTaskRunner> background_sequence,
    const base::FilePath& download_cache,
    const std::string& session_identifier) {
  return base::MakeRefCounted<BackgroundDownloaderSharedSessionProxy>(
      background_sequence, download_cache, session_identifier);
}

}  // namespace update_client
