// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_net_worker.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/net/network_file_fetcher.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace updater {

namespace {

// Creates a `PostRequestObserver` remote with the given callback and put it
// into a thin wrapper for ref-counting.
class PostRequestObserverWrapper
    : public base::RefCountedThreadSafe<PostRequestObserverWrapper> {
 public:
  explicit PostRequestObserverWrapper(
      mojom::FetchService::PostRequestCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(observer_.BindNewPipeAndPassReceiver());
  }

  void OnResponseStarted(int32_t http_status_code, int64_t content_length) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_->OnResponseStarted(http_status_code, content_length);
  }

  void OnProgress(int64_t current) { observer_->OnProgress(current); }

  void OnRequestComplete(std::unique_ptr<std::string> response_body,
                         int32_t net_error,
                         const std::string& header_etag,
                         const std::string& header_x_cup_server_proof,
                         int64_t xheader_retry_after_sec) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_->OnRequestComplete(*response_body, net_error, header_etag,
                                 header_x_cup_server_proof,
                                 xheader_retry_after_sec);
  }

 private:
  friend class base::RefCountedThreadSafe<PostRequestObserverWrapper>;

  virtual ~PostRequestObserverWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<mojom::PostRequestObserver> observer_;
};

// Creates a `FileDownloadObserver` remote with the given callback and put it
// into a thin wrapper for ref-counting.
class FileDownloadObserverWrapper
    : public base::RefCountedThreadSafe<FileDownloadObserverWrapper> {
 public:
  explicit FileDownloadObserverWrapper(
      mojom::FetchService::DownloadToFileCallback callback) {
    std::move(callback).Run(observer_.BindNewPipeAndPassReceiver());
  }

  void OnResponseStarted(int32_t http_status_code, int64_t content_length) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_->OnResponseStarted(http_status_code, content_length);
  }

  void OnProgress(int64_t current) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_->OnProgress(current);
  }

  void OnDownloadComplete(int32_t net_error, int64_t content_length) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_->OnDownloadComplete(net_error, content_length);
  }

 private:
  friend class base::RefCountedThreadSafe<FileDownloadObserverWrapper>;

  virtual ~FileDownloadObserverWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<mojom::FileDownloadObserver> observer_;
};

// The stub class that translates and forwards the Mojo requests to the
// underlying fetcher. It also keeps a reference to the remote receiver and
// sends the result back when fetch is done.
class FetchServiceImpl : public mojom::FetchService {
 public:
  FetchServiceImpl(mojo::PendingReceiver<mojom::FetchService> pending_receiver,
                   base::OnceCallback<void(int)> on_complete_callback);
  ~FetchServiceImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Overrides for mojom::FetchService.
  void PostRequest(const ::GURL& url,
                   const std::string& post_data,
                   const std::string& content_type,
                   std::vector<mojom::HttpHeaderPtr> additional_headers,
                   mojom::FetchService::PostRequestCallback callback) override;

  void DownloadToFile(
      const ::GURL& url,
      ::base::File output_file,
      mojom::FetchService::DownloadToFileCallback callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<mojom::FetchService> receiver_;
  base::OnceCallback<void(int)> on_complete_callback_;

  // Network fetcher for POST request.
  std::unique_ptr<update_client::NetworkFetcher> fetcher_;

  // For file download, `update_client::NetworkFetcher` interface takes
  // a `base::FilePath` as the output, and the Mojo interface takes a
  // `base::File` object. This customized fetcher is used to support
  // the Mojo interface.
  std::unique_ptr<NetworkFileFetcher> file_fetcher_;
};

FetchServiceImpl::FetchServiceImpl(
    mojo::PendingReceiver<mojom::FetchService> pending_receiver,
    base::OnceCallback<void(int)> on_complete_callback)
    : receiver_(this, std::move(pending_receiver)),
      on_complete_callback_(std::move(on_complete_callback)) {}

void FetchServiceImpl::PostRequest(
    const ::GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    std::vector<mojom::HttpHeaderPtr> additional_headers,
    mojom::FetchService::PostRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto wrapper =
      base::MakeRefCounted<PostRequestObserverWrapper>(std::move(callback));
  if (fetcher_ || file_fetcher_) {
    LOG(ERROR) << "Each service instance can do only one fetch request.";
    wrapper->OnRequestComplete(nullptr, kErrorMojoRequestRejected, {}, {}, -1);
    std::move(on_complete_callback_).Run(kErrorMojoRequestRejected);
    return;
  }
  base::flat_map<std::string, std::string> headers;
  for (const auto& header : additional_headers) {
    headers.emplace(header->name, header->value);
  }
  // Creates a network fetcher without any proxy configuration (let the system
  // handle the proxy settings) to fetch data.
  fetcher_ =
      base::MakeRefCounted<NetworkFetcherFactory>(std::nullopt)->Create();
  fetcher_->PostRequest(
      url, post_data, content_type, headers,
      base::BindRepeating(&PostRequestObserverWrapper::OnResponseStarted,
                          wrapper),
      base::BindRepeating(&PostRequestObserverWrapper::OnProgress, wrapper),
      base::BindOnce(
          [](scoped_refptr<PostRequestObserverWrapper> wrapper,
             base::OnceCallback<void(int)> callback,
             std::unique_ptr<std::string> response_body, int32_t net_error,
             const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             int64_t xheader_retry_after_sec) {
            wrapper->OnRequestComplete(std::move(response_body), net_error,
                                       header_etag, header_x_cup_server_proof,
                                       xheader_retry_after_sec);
            std::move(callback).Run(net_error);
          },
          wrapper, std::move(on_complete_callback_)));
}

void FetchServiceImpl::DownloadToFile(
    const ::GURL& url,
    ::base::File output_file,
    mojom::FetchService::DownloadToFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto wrapper =
      base::MakeRefCounted<FileDownloadObserverWrapper>(std::move(callback));
  if (fetcher_ || file_fetcher_) {
    LOG(ERROR) << "Each service instance can do only one fetch request.";
    wrapper->OnDownloadComplete(kErrorMojoRequestRejected, -1);
    std::move(on_complete_callback_).Run(kErrorMojoRequestRejected);
    return;
  }
  file_fetcher_ = std::make_unique<NetworkFileFetcher>();
  file_fetcher_->Download(
      url, std::move(output_file),
      base::BindRepeating(&FileDownloadObserverWrapper::OnResponseStarted,
                          wrapper),
      base::BindRepeating(&FileDownloadObserverWrapper::OnProgress, wrapper),
      base::BindOnce(
          [](scoped_refptr<FileDownloadObserverWrapper> wrapper,
             base::OnceCallback<void(int)> callback, int32_t net_error,
             int64_t content_length) {
            wrapper->OnDownloadComplete(net_error, content_length);
            std::move(callback).Run(net_error);
          },
          wrapper, std::move(on_complete_callback_)));
}

// AppNetWorker runs networking tasks in a dedicated process.
class AppNetWorker : public App {
 public:
  AppNetWorker() {
    net_thread_.StartWithOptions({base::MessagePumpType::IO, 0});
  }

 private:
  ~AppNetWorker() override = default;

  void FirstTaskRun() override {
    // This process must be started with the command line switch
    /// `--mojo-platform-channel-handle=N`. In other words, the command line
    // must be prepared by
    // `mojo::PlatformChannel::PrepareToPassRemoteEndpoint()`.
    mojo::PlatformChannelEndpoint endpoint =
        mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            *base::CommandLine::ForCurrentProcess());
    if (!endpoint.is_valid()) {
      Shutdown(kErrorMojoConnectionFailure);
      return;
    }

    mojo::ScopedMessagePipeHandle pipe =
        mojo::IncomingInvitation::AcceptIsolated(std::move(endpoint));
    if (!pipe->is_valid()) {
      Shutdown(kErrorMojoConnectionFailure);
      return;
    }

    fetcher_stub_ = base::SequenceBound<FetchServiceImpl>(
        net_thread_.task_runner(),
        mojo::PendingReceiver<mojom::FetchService>(std::move(pipe)),
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &AppNetWorker::Shutdown, weak_ptr_factory_.GetWeakPtr())));

    // TODO(crbug.com/353751917): Add a timer that shutdown this process if
    // no incoming network requests in time.
  }

  base::Thread net_thread_{"Network"};
  base::SequenceBound<FetchServiceImpl> fetcher_stub_;
  base::WeakPtrFactory<AppNetWorker> weak_ptr_factory_{this};
};

}  // namespace

scoped_refptr<App> MakeAppNetWorker() {
  return base::MakeRefCounted<AppNetWorker>();
}

}  // namespace updater
