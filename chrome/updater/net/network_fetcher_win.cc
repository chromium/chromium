// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include <windows.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/scoped_impersonation.h"
#include "chrome/updater/win/user_info.h"
#include "components/update_client/network.h"
#include "components/winhttp/proxy_configuration.h"
#include "components/winhttp/scoped_hinternet.h"
#include "url/gurl.h"

namespace updater {
namespace {

// Factory method for the proxy configuration strategy.
scoped_refptr<winhttp::ProxyConfiguration> GetProxyConfiguration(
    std::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration) {
  if (policy_service_proxy_configuration) {
    VLOG(1) << "Using cloud policy configuration for proxy.";
    return base::MakeRefCounted<winhttp::ProxyConfiguration>(winhttp::ProxyInfo{
        policy_service_proxy_configuration->proxy_auto_detect,
        base::SysUTF8ToWide(
            policy_service_proxy_configuration->proxy_pac_url.value_or("")),
        base::SysUTF8ToWide(
            policy_service_proxy_configuration->proxy_url.value_or("")),
        L""});
  }
  VLOG(1) << "Using the system configuration for proxy.";
  return base::MakeRefCounted<winhttp::AutoProxyConfiguration>();
}

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  NetworkFetcher(scoped_refptr<winhttp::SharedHInternet> session_handle,
                 scoped_refptr<winhttp::ProxyConfiguration> proxy_config);
  ~NetworkFetcher() override;
  NetworkFetcher(const NetworkFetcher&) = delete;
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void PostRequestComplete(int response_code);
  void DownloadToFileComplete(int response_code);

  scoped_refptr<winhttp::NetworkFetcher> winhttp_network_fetcher_;

  DownloadToFileCompleteCallback download_to_file_complete_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;
};

NetworkFetcher::NetworkFetcher(
    scoped_refptr<winhttp::SharedHInternet> session_handle,
    scoped_refptr<winhttp::ProxyConfiguration> proxy_config)
    : winhttp_network_fetcher_(
          base::MakeRefCounted<winhttp::NetworkFetcher>(session_handle,
                                                        proxy_config)) {}

NetworkFetcher::~NetworkFetcher() {
  winhttp_network_fetcher_->Close();
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
  VLOG(2) << __func__;
  post_request_complete_callback_ = std::move(post_request_complete_callback);
  winhttp_network_fetcher_->PostRequest(
      url, post_data, content_type, post_additional_headers,
      std::move(response_started_callback), std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::PostRequestComplete,
                     base::Unretained(this)));
}

base::OnceClosure NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  download_to_file_complete_callback_ =
      std::move(download_to_file_complete_callback);
  return winhttp_network_fetcher_->DownloadToFile(
      url, file_path, std::move(response_started_callback),
      std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::DownloadToFileComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::PostRequestComplete(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__ << ": response code=" << response_code;

  // Attempt to get some response headers.  Not all headers may be present so
  // this is best effort only.
  std::wstring x_cup_server_proof;
  std::wstring etag;
  int x_retry_after_sec = -1;
  winhttp_network_fetcher_->QueryHeaderString(
      base::SysUTF8ToWide(
          update_client::NetworkFetcher::kHeaderXCupServerProof),
      &x_cup_server_proof);
  winhttp_network_fetcher_->QueryHeaderString(
      base::SysUTF8ToWide(update_client::NetworkFetcher::kHeaderEtag), &etag);
  winhttp_network_fetcher_->QueryHeaderInt(
      base::SysUTF8ToWide(update_client::NetworkFetcher::kHeaderXRetryAfter),
      &x_retry_after_sec);

  std::move(post_request_complete_callback_)
      .Run(std::make_unique<std::string>(
               winhttp_network_fetcher_->GetResponseBody()),
           winhttp_network_fetcher_->GetNetError(), base::SysWideToUTF8(etag),
           base::SysWideToUTF8(x_cup_server_proof), x_retry_after_sec);
}

void NetworkFetcher::DownloadToFileComplete(int /*response_code*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  std::move(download_to_file_complete_callback_)
      .Run(winhttp_network_fetcher_->GetNetError(),
           winhttp_network_fetcher_->GetContentSize());
}

}  // namespace

class NetworkFetcherFactory::Impl {
 public:
  explicit Impl(std::optional<PolicyServiceProxyConfiguration>
                    policy_service_proxy_configuration)
      : proxy_configuration_(
            GetProxyConfiguration(policy_service_proxy_configuration)) {
    ScopedImpersonation impersonate;
    if (IsSystemInstall()) {
      HResultOr<ScopedKernelHANDLE> token = GetLoggedOnUserToken();
      VLOG_IF(2, !token.has_value())
          << __func__ << ": GetLoggedOnUserToken failed: " << std::hex
          << token.error();
      if (token.has_value()) {
        const HRESULT hr = impersonate.Impersonate(token.value().get());
        VLOG(2)
            << __func__
            << ": Successfully got logged on user token. Impersonate result: "
            << std::hex << hr;
      }
    }
    session_handle_ = base::MakeRefCounted<winhttp::SharedHInternet>(
        winhttp::CreateSessionHandle(base::SysUTF8ToWide(GetUpdaterUserAgent()),
                                     proxy_configuration_->access_type(),
                                     proxy_configuration_->proxy(),
                                     proxy_configuration_->proxy_bypass()));
    VLOG_IF(2, !session_handle_) << "Failed to create a winhttp session.";
  }

  std::unique_ptr<update_client::NetworkFetcher> Create() {
    return session_handle_ ? std::make_unique<NetworkFetcher>(
                                 session_handle_, proxy_configuration_)
                           : nullptr;
  }

 private:
  scoped_refptr<winhttp::ProxyConfiguration> proxy_configuration_;
  scoped_refptr<winhttp::SharedHInternet> session_handle_;
};

NetworkFetcherFactory::NetworkFetcherFactory(
    std::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration)
    : impl_(std::make_unique<Impl>(policy_service_proxy_configuration)) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return impl_->Create();
}

}  // namespace updater
