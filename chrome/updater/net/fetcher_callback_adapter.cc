// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/fetcher_callback_adapter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/process/process.h"
#include "base/task/bind_post_task.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

using ResponseStartedCallback =
    update_client::NetworkFetcher::ResponseStartedCallback;
using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
using PostRequestCompleteCallback =
    update_client::NetworkFetcher::PostRequestCompleteCallback;
using DownloadToFileCompleteCallback =
    update_client::NetworkFetcher::DownloadToFileCompleteCallback;

namespace updater {
namespace {

class PostRequestObserverImpl : public mojom::PostRequestObserver {
 public:
  PostRequestObserverImpl(
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback)
      : response_started_callback_(std::move(response_started_callback)),
        progress_callback_(std::move(progress_callback)),
        post_request_complete_callback_(
            std::move(post_request_complete_callback)) {}
  PostRequestObserverImpl(const PostRequestObserverImpl&) = delete;
  PostRequestObserverImpl& operator=(const PostRequestObserverImpl&) = delete;
  ~PostRequestObserverImpl() override = default;

  // Overrides for mojom::PostRequestObserver.
  void OnResponseStarted(uint32_t http_status_code,
                         std::optional<uint64_t> content_length) override {
    response_started_callback_.Run(
        ToSignedIntegral(http_status_code),
        content_length ? ToSignedIntegral(*content_length) : -1);
  }
  void OnProgress(uint64_t current) override {
    progress_callback_.Run(ToSignedIntegral(current));
  }
  void OnRequestComplete(
      const std::string& response_body,
      int32_t net_error,
      const std::string& header_etag,
      const std::string& header_x_cup_server_proof,
      std::optional<uint64_t> xheader_retry_after_sec) override {
    CHECK(post_request_complete_callback_)
        << __func__ << " is called without a valid callback. Was " << __func__
        << " called mulitple times?";
    std::move(post_request_complete_callback_)
        .Run(std::make_unique<std::string>(response_body), net_error,
             header_etag, header_x_cup_server_proof,
             xheader_retry_after_sec
                 ? ToSignedIntegral(*xheader_retry_after_sec)
                 : -1);
  }

 private:
  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;
};

class FileDownloadObserverImpl : public mojom::FileDownloadObserver {
 public:
  FileDownloadObserverImpl(
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_complete_callback)
      : response_started_callback_(response_started_callback),
        progress_callback_(progress_callback),
        download_complete_callback_(std::move(download_complete_callback)) {}
  FileDownloadObserverImpl(const FileDownloadObserverImpl&) = delete;
  FileDownloadObserverImpl& operator=(const FileDownloadObserverImpl&) = delete;
  ~FileDownloadObserverImpl() override = default;

  // Overrides for mojom::FileDownloadObserver.
  void OnResponseStarted(uint32_t http_status_code,
                         std::optional<uint64_t> content_length) override {
    response_started_callback_.Run(
        ToSignedIntegral(http_status_code),
        content_length ? ToSignedIntegral(*content_length) : -1);
  }
  void OnProgress(uint64_t current) override {
    progress_callback_.Run(ToSignedIntegral(current));
  }
  void OnDownloadComplete(int32_t net_error,
                          std::optional<uint64_t> content_length) override {
    CHECK(download_complete_callback_)
        << __func__ << " is called without a valid callback. Was " << __func__
        << " called mulitple times?";
    std::move(download_complete_callback_)
        .Run(net_error,
             content_length ? ToSignedIntegral(*content_length) : -1);
  }

 private:
  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;
  DownloadToFileCompleteCallback download_complete_callback_;
};

}  // namespace

// Binds a callback which creates a self-owned PostRequestObserverImpl to
// forward RPC callbacks to the provided native callbacks.
base::OnceCallback<void(mojo::PendingReceiver<mojom::PostRequestObserver>)>
MakePostRequestObserver(
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  return base::BindOnce(
      [](ResponseStartedCallback response_started_callback,
         ProgressCallback progress_callback,
         PostRequestCompleteCallback post_request_complete_callback,
         mojo::PendingReceiver<mojom::PostRequestObserver> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<PostRequestObserverImpl>(
                response_started_callback, progress_callback,
                std::move(post_request_complete_callback)),
            std::move(receiver));
      },
      base::BindPostTaskToCurrentDefault(response_started_callback),
      base::BindPostTaskToCurrentDefault(progress_callback),
      base::BindPostTaskToCurrentDefault(
          std::move(post_request_complete_callback)));
}

base::OnceCallback<void(mojo::PendingReceiver<mojom::FileDownloadObserver>)>
MakeFileDownloadObserver(
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_complete_callback) {
  return base::BindOnce(
      [](ResponseStartedCallback response_started_callback,
         ProgressCallback progress_callback,
         DownloadToFileCompleteCallback download_complete_callback,
         mojo::PendingReceiver<mojom::FileDownloadObserver> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<FileDownloadObserverImpl>(
                response_started_callback, progress_callback,
                std::move(download_complete_callback)),
            std::move(receiver));
      },
      base::BindPostTaskToCurrentDefault(response_started_callback),
      base::BindPostTaskToCurrentDefault(progress_callback),
      base::BindPostTaskToCurrentDefault(
          std::move(download_complete_callback)));
}

}  // namespace updater
