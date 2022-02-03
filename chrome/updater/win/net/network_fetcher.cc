// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/win/net/network.h"
#include "chrome/updater/win/scoped_impersonation.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_util.h"
#include "components/update_client/network.h"
#include "components/winhttp/network_fetcher.h"
#include "components/winhttp/proxy_configuration.h"

namespace updater {

namespace {

std::wstring FromCharOrEmpty(const wchar_t* str) {
  return str ? std::wstring(str) : std::wstring();
}

// Wrapper for WINHTTP_CURRENT_USER_IE_PROXY_CONFIG structure.
// According to MSDN, callers must free strings with GlobalFree.
class ScopedIeProxyConfig {
 public:
  ScopedIeProxyConfig();
  ScopedIeProxyConfig(const ScopedIeProxyConfig&) = delete;
  ScopedIeProxyConfig& operator=(const ScopedIeProxyConfig&) = delete;
  ~ScopedIeProxyConfig();

  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* receive() { return &ie_proxy_config_; }

  bool auto_detect() const { return ie_proxy_config_.fAutoDetect; }
  std::wstring auto_config_url() const {
    return FromCharOrEmpty(ie_proxy_config_.lpszAutoConfigUrl);
  }
  std::wstring proxy() const {
    return FromCharOrEmpty(ie_proxy_config_.lpszProxy);
  }
  std::wstring proxy_bypass() const {
    return FromCharOrEmpty(ie_proxy_config_.lpszProxyBypass);
  }

 private:
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_proxy_config_ = {};
};

ScopedIeProxyConfig::ScopedIeProxyConfig() {
  ie_proxy_config_.fAutoDetect = false;
  ie_proxy_config_.lpszAutoConfigUrl = nullptr;
  ie_proxy_config_.lpszProxy = nullptr;
  ie_proxy_config_.lpszProxyBypass = nullptr;
}

ScopedIeProxyConfig::~ScopedIeProxyConfig() {
  if (ie_proxy_config_.lpszAutoConfigUrl)
    ::GlobalFree(ie_proxy_config_.lpszAutoConfigUrl);

  if (ie_proxy_config_.lpszProxy)
    ::GlobalFree(ie_proxy_config_.lpszProxy);

  if (ie_proxy_config_.lpszProxyBypass)
    ::GlobalFree(ie_proxy_config_.lpszProxyBypass);
}

// Factory method for the proxy configuration strategy.
scoped_refptr<winhttp::ProxyConfiguration> GetProxyConfiguration(
    scoped_refptr<PolicyService> policy_service) {
  std::string policy_proxy_mode;
  if (policy_service->GetProxyMode(nullptr, &policy_proxy_mode) &&
      policy_proxy_mode.compare(kProxyModeSystem) != 0) {
    VLOG(2) << "Using policy proxy " << policy_proxy_mode;
    bool auto_detect = false;
    std::wstring pac_url;
    std::wstring proxy_url;
    bool is_policy_config_valid = true;

    if (policy_proxy_mode.compare(kProxyModeFixedServers) == 0) {
      std::string policy_proxy_url;
      if (!policy_service->GetProxyServer(nullptr, &policy_proxy_url)) {
        VLOG(1) << "Fixed server mode proxy has no URL specified.";
        is_policy_config_valid = false;
      } else {
        proxy_url = base::SysUTF8ToWide(policy_proxy_url);
      }
    } else if (policy_proxy_mode.compare(kProxyModePacScript) == 0) {
      std::string policy_pac_url;
      if (!policy_service->GetProxyServer(nullptr, &policy_pac_url)) {
        VLOG(1) << "PAC proxy policy has no PAC URL specified.";
        is_policy_config_valid = false;
      } else {
        pac_url = base::SysUTF8ToWide(policy_pac_url);
      }
    } else if (policy_proxy_mode.compare(kProxyModeAutoDetect)) {
      auto_detect = true;
    }

    if (is_policy_config_valid) {
      return base::MakeRefCounted<winhttp::ProxyConfiguration>(
          winhttp::ProxyInfo{auto_detect, pac_url, proxy_url, L""});
    } else {
      VLOG(1) << "Configuration set by policy was invalid."
              << "Proceding with system configuration";
    }
  }

  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  const bool supports_automatic_proxy =
      os_info->version() >= base::win::Version::WIN8_1;
  if (supports_automatic_proxy)
    return base::MakeRefCounted<winhttp::AutoProxyConfiguration>();

  ScopedImpersonation impersonate_user;
  if (IsLocalSystemUser()) {
    VLOG(2) << "Running as SYSTEM, impersonate the current user.";
    base::win::ScopedHandle user_token = GetUserTokenFromCurrentSessionId();
    if (user_token.IsValid()) {
      impersonate_user.Impersonate(user_token.Get());
    }
  }

  ScopedIeProxyConfig ie_proxy_config;
  if (::WinHttpGetIEProxyConfigForCurrentUser(ie_proxy_config.receive())) {
    return base::MakeRefCounted<winhttp::ProxyConfiguration>(winhttp::ProxyInfo{
        ie_proxy_config.auto_detect(), ie_proxy_config.auto_config_url(),
        ie_proxy_config.proxy(), ie_proxy_config.proxy_bypass()});
  } else {
    PLOG(ERROR) << "Failed to get proxy for current user";
  }

  return base::MakeRefCounted<winhttp::ProxyConfiguration>();
}

}  // namespace

NetworkFetcher::NetworkFetcher(
    const HINTERNET& session_handle,
    scoped_refptr<winhttp::ProxyConfiguration> proxy_config)
    : winhttp_network_fetcher_(
          base::MakeRefCounted<winhttp::NetworkFetcher>(session_handle,
                                                        proxy_config)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

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

void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  download_to_file_complete_callback_ =
      std::move(download_to_file_complete_callback);
  winhttp_network_fetcher_->DownloadToFile(
      url, file_path, std::move(response_started_callback),
      std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::DownloadToFileComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::PostRequestComplete(int /*response_code*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  // Attempt to get some response headers.  Not all headers may be present so
  // this is best effort only.
  std::wstring x_cup_server_proof;
  std::wstring etag;
  int x_retry_after_sec = 0;
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

NetworkFetcherFactory::NetworkFetcherFactory(
    scoped_refptr<PolicyService> policy_service)
    : proxy_configuration_(GetProxyConfiguration(policy_service)),
      session_handle_(
          winhttp::CreateSessionHandle(L"Chrome Updater",
                                       proxy_configuration_->access_type())) {}

NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_handle_.get()
             ? std::make_unique<NetworkFetcher>(session_handle_.get(),
                                                proxy_configuration_)
             : nullptr;
}

}  // namespace updater
