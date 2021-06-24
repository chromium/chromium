// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network_fetcher.h"

#include <versionhelpers.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/win/net/network.h"
#include "chrome/updater/win/net/network_winhttp.h"
#include "chrome/updater/win/net/proxy_configuration.h"

namespace updater {

NetworkFetcher::NetworkFetcher(const HINTERNET& session_handle,
                               scoped_refptr<ProxyConfiguration> proxy_config)
    : network_fetcher_(
          base::MakeRefCounted<NetworkFetcherWinHTTP>(session_handle,
                                                      proxy_config)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

NetworkFetcher::~NetworkFetcher() {
  network_fetcher_->Close();
}

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  post_request_complete_callback_ = std::move(post_request_complete_callback);
  network_fetcher_->PostRequest(
      url, post_data, content_type, post_additional_headers,
      std::move(response_started_callback), std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::PostRequestComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_to_file_complete_callback_ =
      std::move(download_to_file_complete_callback);
  network_fetcher_->DownloadToFile(
      url, file_path, std::move(response_started_callback),
      std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::DownloadToFileComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::PostRequestComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(post_request_complete_callback_)
      .Run(std::make_unique<std::string>(network_fetcher_->GetResponseBody()),
           network_fetcher_->GetNetError(), network_fetcher_->GetHeaderETag(),
           network_fetcher_->GetHeaderXCupServerProof(),
           network_fetcher_->GetHeaderXRetryAfterSec());
}

void NetworkFetcher::DownloadToFileComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(download_to_file_complete_callback_)
      .Run(network_fetcher_->GetNetError(), network_fetcher_->GetContentSize());
}

NetworkFetcherFactory::NetworkFetcherFactory(
    scoped_refptr<PolicyService> policy_service)
    : proxy_configuration_(GetProxyConfiguration(policy_service)),
      session_handle_(
          CreateSessionHandle(proxy_configuration_->access_type())) {}

NetworkFetcherFactory::~NetworkFetcherFactory() = default;

scoped_hinternet NetworkFetcherFactory::CreateSessionHandle(
    int proxy_access_type) {
  scoped_hinternet session_handle(
      ::WinHttpOpen(L"Chrome Updater", proxy_access_type, WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));

  // Allow TLS1.2 on Windows 7 and Windows 8. See KB3140245. TLS 1.2 is enabled
  // by default on Windows 8.1 and Windows 10.
  if (session_handle.is_valid() && ::IsWindows7OrGreater() &&
      !::IsWindows8Point1OrGreater()) {
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 |
                      WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                      WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    ::WinHttpSetOption(session_handle.get(), WINHTTP_OPTION_SECURE_PROTOCOLS,
                       &protocols, sizeof(protocols));
  }
  return session_handle;
}

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_handle_.get()
             ? std::make_unique<NetworkFetcher>(session_handle_.get(),
                                                proxy_configuration_)
             : nullptr;
}

}  // namespace updater
