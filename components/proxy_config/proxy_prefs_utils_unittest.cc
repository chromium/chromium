// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_prefs_utils.h"

#include <string>

#include "components/policy/core/common/policy_types.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/base/proxy_chain.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace proxy_config {

TEST(ProxyPrefsUtilsTest, ProxyOverrideRuleHostFromString) {
  url::SchemeHostPort scheme_host_port =
      ProxyOverrideRuleHostFromString("https://google.com:123");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpsScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 123);

  scheme_host_port = ProxyOverrideRuleHostFromString("google.com:123");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 123);

  scheme_host_port = ProxyOverrideRuleHostFromString("https://google.com");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpsScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 443);

  scheme_host_port = ProxyOverrideRuleHostFromString("google.com");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpScheme);
  ASSERT_EQ(scheme_host_port.host(), "google.com");
  ASSERT_EQ(scheme_host_port.port(), 80);

  scheme_host_port = ProxyOverrideRuleHostFromString("192.168.1.1");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpScheme);
  ASSERT_EQ(scheme_host_port.host(), "192.168.1.1");
  ASSERT_EQ(scheme_host_port.port(), 80);

  scheme_host_port =
      ProxyOverrideRuleHostFromString("https://[3ffe:2a00:100:7031:0:0::1]");
  ASSERT_TRUE(scheme_host_port.IsValid());
  ASSERT_EQ(scheme_host_port.scheme(), url::kHttpsScheme);
  ASSERT_EQ(scheme_host_port.host(), "[3ffe:2a00:100:7031::1]");
  ASSERT_EQ(scheme_host_port.port(), 443);

  ASSERT_FALSE(ProxyOverrideRuleHostFromString("https://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleHostFromString("http://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleHostFromString("://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleHostFromString("123456789").IsValid());
}

TEST(ProxyPrefsUtilsTest, ProxyOverrideRuleProxyFromString) {
  net::ProxyChain proxy_chain =
      ProxyOverrideRuleProxyFromString("https://google.com:123");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 123);

  proxy_chain = ProxyOverrideRuleProxyFromString("https://google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 443);

  proxy_chain = ProxyOverrideRuleProxyFromString("HTTPS google.com:123");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 123);

  proxy_chain = ProxyOverrideRuleProxyFromString("HTTPS google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTPS);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 443);

  proxy_chain = ProxyOverrideRuleProxyFromString("socks4://google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS4);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("SOCKS4 google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS4);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("socks5://google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS5);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("SOCKS5 google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_SOCKS5);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 1080);

  proxy_chain = ProxyOverrideRuleProxyFromString("PROXY google.com");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_EQ(proxy_chain.First().scheme(), net::ProxyServer::SCHEME_HTTP);
  ASSERT_EQ(proxy_chain.First().GetHost(), "google.com");
  ASSERT_EQ(proxy_chain.First().GetPort(), 80);

  proxy_chain = ProxyOverrideRuleProxyFromString("DIRECT");
  ASSERT_TRUE(proxy_chain.IsValid());
  ASSERT_TRUE(proxy_chain.is_direct());

  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("google").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("google.com").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("https://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("http://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("://").IsValid());
  ASSERT_FALSE(ProxyOverrideRuleProxyFromString("123456789").IsValid());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST(ProxyPrefsUtilsTest, ProxyOverrideRulesAllowed) {
  TestingPrefServiceSimple prefs;
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(prefs.registry());

  // If rules are set at the machine scope, they are always allowed.
  prefs.SetInteger(prefs::kProxyOverrideRulesScope,
                   policy::POLICY_SCOPE_MACHINE);
  for (bool affiliation : {true, false}) {
    for (int enabled_for_all_users : {0, 1}) {
      prefs.SetBoolean(prefs::kProxyOverrideRulesAffiliation, affiliation);
      prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers,
                       enabled_for_all_users);
      EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs));
    }
  }

  // If rules are set at the user scope, they are always allowed for affiliated
  // users.
  prefs.SetInteger(prefs::kProxyOverrideRulesScope, policy::POLICY_SCOPE_USER);
  prefs.SetBoolean(prefs::kProxyOverrideRulesAffiliation, true);
  for (int enabled_for_all_users : {0, 1}) {
    prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers,
                     enabled_for_all_users);
    EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs));
  }

  // If rules are set at the user scope for an unaffiliated user, they are only
  // allowed when `kEnableProxyOverrideRulesForAllUsers` is set to 1.
  prefs.SetBoolean(prefs::kProxyOverrideRulesAffiliation, false);

  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 0);
  EXPECT_FALSE(ProxyOverrideRulesAllowed(&prefs));

  prefs.SetInteger(prefs::kEnableProxyOverrideRulesForAllUsers, 1);
  EXPECT_TRUE(ProxyOverrideRulesAllowed(&prefs));
}
#endif

}  // namespace proxy_config
