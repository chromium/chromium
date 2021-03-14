// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_PROXY_CONFIGURATION_H_
#define CHROME_UPDATER_WIN_NET_PROXY_CONFIGURATION_H_

#include <windows.h>
#include <winhttp.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chrome/updater/win/net/proxy_info.h"
#include "chrome/updater/win/net/scoped_winttp_proxy_info.h"

class GURL;

namespace updater {

// Classes in this module represent sources of system proxy configuration.
// On Windows 8.1 and above, we can use Auto Proxy mode in WinHTTP and let
// the OS configure the proxy.
// This is represented by the AutoProxyConfiguration class.
// When a proxy related policy is set or when using Windows 8 or below,
// we'd set proxy manually using ProxyConfiguration class.
//
// Default WinHTTP proxy strategy - provide proxy server info to WinHTTP.
// Used when policy is set or on Windows 8 and below.
// This class can use either provided proxy information from the
// IE Config or a policy policy setting or query per request proxy info
// with WPAD (Web Proxy Auto Discovery).
class ProxyConfiguration : public base::RefCounted<ProxyConfiguration> {
 public:
  ProxyConfiguration() = default;
  explicit ProxyConfiguration(const ProxyInfo& proxy_info);
  ProxyConfiguration(const ProxyConfiguration&) = delete;
  ProxyConfiguration& operator=(const ProxyConfiguration&) = delete;

  int access_type() const;
  base::Optional<ScopedWinHttpProxyInfo> GetProxyForUrl(
      HINTERNET session_handle,
      const GURL& url) const;

 protected:
  virtual ~ProxyConfiguration() = default;

 private:
  friend class base::RefCounted<ProxyConfiguration>;

  virtual int DoGetAccessType() const;
  virtual base::Optional<ScopedWinHttpProxyInfo> DoGetProxyForUrl(
      HINTERNET session_handle,
      const GURL& url) const;

  ProxyInfo proxy_info_;
};

// Auto proxy strategy - let WinHTTP detect proxy settings.
// This is only available on Windows 8.1 and above.
class AutoProxyConfiguration final : public ProxyConfiguration {
 protected:
  ~AutoProxyConfiguration() override = default;

 private:
  // Overrides for ProxyConfiguration.
  int DoGetAccessType() const override;
  base::Optional<ScopedWinHttpProxyInfo> DoGetProxyForUrl(
      HINTERNET session_handle,
      const GURL& url) const override;
};

// Sets proxy info on a request handle, if WINHTTP_PROXY_INFO is provided.
void SetProxyForRequest(
    const HINTERNET request_handle,
    const base::Optional<ScopedWinHttpProxyInfo>& winhttp_proxy_info);

// Factory method for the proxy configuration strategy.
scoped_refptr<ProxyConfiguration> GetProxyConfiguration();

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_PROXY_CONFIGURATION_H_
