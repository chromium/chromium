// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/proxy_configuration.h"

#include "base/memory/scoped_refptr.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

TEST(ProxyConfiguration, DirectProxy) {
  auto proxy_configuration = base::MakeRefCounted<ProxyConfiguration>();
  EXPECT_EQ(proxy_configuration->access_type(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY);
}

TEST(ProxyConfiguration, AutoProxy) {
  auto proxy_configuration = base::MakeRefCounted<AutoProxyConfiguration>();
  EXPECT_EQ(proxy_configuration->access_type(),
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY);
  base::Optional<ScopedWinHttpProxyInfo> winhttp_proxy_info =
      proxy_configuration->GetProxyForUrl(nullptr, GURL("http://example.com"));
  EXPECT_FALSE(winhttp_proxy_info.has_value());
}

TEST(ProxyConfiguration, NamedProxy) {
  auto proxy_configuration = base::MakeRefCounted<ProxyConfiguration>(
      ProxyInfo(false, L"", L"http://192.168.0.1", L""));
  EXPECT_EQ(proxy_configuration->access_type(),
            WINHTTP_ACCESS_TYPE_NAMED_PROXY);
}

TEST(ProxyConfiguration, WPADProxyGetProxyForUrl) {
  auto proxy_configuration =
      base::MakeRefCounted<ProxyConfiguration>(ProxyInfo(true, L"", L"", L""));
  EXPECT_EQ(proxy_configuration->access_type(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY);
  base::Optional<ScopedWinHttpProxyInfo> winhttp_proxy_info =
      proxy_configuration->GetProxyForUrl(nullptr, GURL("http://example.com"));
  EXPECT_FALSE(winhttp_proxy_info.has_value());
}

}  // namespace updater
