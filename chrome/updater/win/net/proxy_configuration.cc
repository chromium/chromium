// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/proxy_configuration.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy_manager.h"
#include "chrome/updater/win/net/net_util.h"
#include "chrome/updater/win/net/proxy_info.h"
#include "chrome/updater/win/net/scoped_winttp_proxy_info.h"
#include "chrome/updater/win/scoped_impersonation.h"
#include "chrome/updater/win/user_info.h"
#include "url/gurl.h"

namespace updater {

namespace {

std::wstring FromCharOrEmpty(const wchar_t* str) {
  return str ? std::wstring(str) : L"";
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

}  // namespace

ProxyConfiguration::ProxyConfiguration(const ProxyInfo& proxy_info)
    : proxy_info_(proxy_info) {}

int ProxyConfiguration::access_type() const {
  return DoGetAccessType();
}

int ProxyConfiguration::DoGetAccessType() const {
  const bool is_using_named_proxy = !proxy_info_.auto_detect &&
                                    proxy_info_.auto_config_url.empty() &&
                                    !proxy_info_.proxy.empty();

  return is_using_named_proxy ? WINHTTP_ACCESS_TYPE_NAMED_PROXY
                              : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
}

base::Optional<ScopedWinHttpProxyInfo> ProxyConfiguration::GetProxyForUrl(
    HINTERNET session_handle,
    const GURL& url) const {
  return DoGetProxyForUrl(session_handle, url);
}

base::Optional<ScopedWinHttpProxyInfo> ProxyConfiguration::DoGetProxyForUrl(
    HINTERNET session_handle,
    const GURL& url) const {
  // Detect proxy settings using Web Proxy Auto Detection (WPAD).
  WINHTTP_AUTOPROXY_OPTIONS auto_proxy_options = {0};

  // Per MSDN, setting fAutoLogonIfChallenged to false first may work
  // if Windows cached the proxy config.
  auto_proxy_options.fAutoLogonIfChallenged = false;

  bool try_auto_proxy = false;

  if (proxy_info_.auto_detect) {
    auto_proxy_options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    auto_proxy_options.dwAutoDetectFlags =
        WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    try_auto_proxy = true;
  }

  // PAC Url was specified, let system auto detect given the PAC url.
  if (!proxy_info_.auto_config_url.empty()) {
    auto_proxy_options.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
    auto_proxy_options.lpszAutoConfigUrl = proxy_info_.auto_config_url.c_str();
    try_auto_proxy = true;
  }

  // Find the proxy server for the url.
  ScopedWinHttpProxyInfo winhttp_proxy_info = {};
  if (try_auto_proxy) {
    const std::wstring url_str = base::SysUTF8ToWide(url.spec());
    bool success = ::WinHttpGetProxyForUrl(session_handle, url_str.c_str(),
                                           &auto_proxy_options,
                                           winhttp_proxy_info.receive());
    if (!success && ::GetLastError() == ERROR_WINHTTP_LOGIN_FAILURE) {
      auto_proxy_options.fAutoLogonIfChallenged = true;
      success = ::WinHttpGetProxyForUrl(session_handle, url_str.c_str(),
                                        &auto_proxy_options,
                                        winhttp_proxy_info.receive());
    }

    if (!success) {
      PLOG(ERROR) << "Failed to get proxy for url";
      return {};
    }
  } else {
    winhttp_proxy_info.set_proxy(proxy_info_.proxy);
    winhttp_proxy_info.set_proxy_bypass(proxy_info_.proxy_bypass);
  }

  if (!winhttp_proxy_info.IsValid())
    return {};

  return winhttp_proxy_info;
}

void SetProxyForRequest(
    const HINTERNET request_handle,
    const base::Optional<ScopedWinHttpProxyInfo>& winhttp_proxy_info) {
  // Set the proxy option on the request handle.
  if (winhttp_proxy_info.has_value() && winhttp_proxy_info.value().IsValid()) {
    const ScopedWinHttpProxyInfo& proxy_info = winhttp_proxy_info.value();
    VLOG(1) << "Setting proxy " << proxy_info.proxy();
    auto hr = SetOption(request_handle, WINHTTP_OPTION_PROXY,
                        const_cast<WINHTTP_PROXY_INFO*>(proxy_info.get()));
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to set WINHTTP_OPTION_PROXY: 0x" << std::hex << hr;
    }
  }
}

scoped_refptr<ProxyConfiguration> GetProxyConfiguration() {
  std::unique_ptr<PolicyManagerInterface> policy_manager = GetPolicyManager();
  std::string policy_proxy_mode;
  if (policy_manager->GetProxyMode(&policy_proxy_mode) &&
      policy_proxy_mode.compare(kProxyModeSystem) != 0) {
    DVLOG(3) << "Using policy proxy " << policy_proxy_mode;
    bool auto_detect = false;
    std::wstring pac_url;
    std::wstring proxy_url;
    bool is_policy_config_valid = true;

    if (policy_proxy_mode.compare(kProxyModeFixedServers) == 0) {
      std::string policy_proxy_url;
      if (!policy_manager->GetProxyServer(&policy_proxy_url)) {
        VLOG(1) << "Fixed server mode proxy has no URL specified.";
        is_policy_config_valid = false;
      } else {
        proxy_url = base::SysUTF8ToWide(policy_proxy_url);
      }
    } else if (policy_proxy_mode.compare(kProxyModePacScript) == 0) {
      std::string policy_pac_url;
      if (!policy_manager->GetProxyServer(&policy_pac_url)) {
        VLOG(1) << "PAC proxy policy has no PAC URL specified.";
        is_policy_config_valid = false;
      } else {
        pac_url = base::SysUTF8ToWide(policy_pac_url);
      }
    } else if (policy_proxy_mode.compare(kProxyModeAutoDetect)) {
      auto_detect = true;
    }

    if (is_policy_config_valid) {
      return base::MakeRefCounted<ProxyConfiguration>(
          ProxyInfo{auto_detect, pac_url, proxy_url, L""});
    } else {
      VLOG(1) << "Configuration set by policy was invalid."
              << "Proceding with system configuration";
    }
  }

  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  const bool supports_automatic_proxy =
      os_info->version() >= base::win::Version::WIN8_1;
  if (supports_automatic_proxy)
    return base::MakeRefCounted<AutoProxyConfiguration>();

  ScopedImpersonation impersonate_user;
  if (IsLocalSystemUser()) {
    DVLOG(3) << "Running as SYSTEM, impersonate the current user.";
    base::win::ScopedHandle user_token = GetUserTokenFromCurrentSessionId();
    if (user_token.IsValid()) {
      impersonate_user.Impersonate(user_token.Get());
    }
  }

  ScopedIeProxyConfig ie_proxy_config;
  if (::WinHttpGetIEProxyConfigForCurrentUser(ie_proxy_config.receive())) {
    return base::MakeRefCounted<ProxyConfiguration>(ProxyInfo{
        ie_proxy_config.auto_detect(), ie_proxy_config.auto_config_url(),
        ie_proxy_config.proxy(), ie_proxy_config.proxy_bypass()});
  } else {
    DPLOG(ERROR) << "Failed to get proxy for current user";
  }

  return base::MakeRefCounted<ProxyConfiguration>();
}

int AutoProxyConfiguration::DoGetAccessType() const {
  return WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
}

base::Optional<ScopedWinHttpProxyInfo> AutoProxyConfiguration::DoGetProxyForUrl(
    HINTERNET,
    const GURL&) const {
  // When using automatic proxy settings, Windows will resolve the proxy
  // for us.
  DVLOG(3) << "Auto-proxy: skip getting proxy for a url";
  return {};
}

}  // namespace updater
