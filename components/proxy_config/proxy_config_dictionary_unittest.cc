// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_config_dictionary.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

struct ProxyConfigHolder {
  ProxyPrefs::ProxyMode mode;
  std::string pac_url;
  std::string proxy_server;
  std::string bypass_list;
};

TEST(ProxyConfigDictionaryTest, CreateDirect) {
  ProxyConfigDictionary dict(ProxyConfigDictionary::CreateDirect());
  ProxyConfigHolder h;

  ASSERT_TRUE(dict.GetMode(&h.mode));
  EXPECT_EQ(ProxyPrefs::MODE_DIRECT, h.mode);
  ASSERT_FALSE(dict.GetPacUrl(&h.bypass_list));
  ASSERT_FALSE(dict.GetProxyServer(&h.proxy_server));
  ASSERT_FALSE(dict.GetBypassList(&h.bypass_list));
}

TEST(ProxyConfigDictionaryTest, CreateAutoDetect) {
  ProxyConfigDictionary dict(ProxyConfigDictionary::CreateAutoDetect());
  ProxyConfigHolder h;

  ASSERT_TRUE(dict.GetMode(&h.mode));
  EXPECT_EQ(ProxyPrefs::MODE_AUTO_DETECT, h.mode);
  ASSERT_FALSE(dict.GetPacUrl(&h.bypass_list));
  ASSERT_FALSE(dict.GetProxyServer(&h.proxy_server));
  ASSERT_FALSE(dict.GetBypassList(&h.bypass_list));
}

TEST(ProxyConfigDictionaryTest, CreatePacScript) {
  ProxyConfigDictionary dict(
      ProxyConfigDictionary::CreatePacScript("pac", false));
  ProxyConfigHolder h;

  ASSERT_TRUE(dict.GetMode(&h.mode));
  EXPECT_EQ(ProxyPrefs::MODE_PAC_SCRIPT, h.mode);
  ASSERT_TRUE(dict.GetPacUrl(&h.bypass_list));
  EXPECT_EQ("pac", h.bypass_list);
  ASSERT_FALSE(dict.GetProxyServer(&h.proxy_server));
  ASSERT_FALSE(dict.GetBypassList(&h.bypass_list));
}

TEST(ProxyConfigDictionaryTest, CreateFixedServers) {
  ProxyConfigDictionary dict(ProxyConfigDictionary::CreateFixedServers(
      "http://1.2.3.4", "http://foo"));
  ProxyConfigHolder h;

  ASSERT_TRUE(dict.GetMode(&h.mode));
  EXPECT_EQ(ProxyPrefs::MODE_FIXED_SERVERS, h.mode);
  ASSERT_FALSE(dict.GetPacUrl(&h.bypass_list));
  ASSERT_TRUE(dict.GetProxyServer(&h.proxy_server));
  EXPECT_EQ("http://1.2.3.4", h.proxy_server);
  ASSERT_TRUE(dict.GetBypassList(&h.bypass_list));
  EXPECT_EQ("http://foo", h.bypass_list);
}

TEST(ProxyConfigDictionaryTest, CreateSystem) {
  ProxyConfigDictionary dict(ProxyConfigDictionary::CreateSystem());
  ProxyConfigHolder h;

  ASSERT_TRUE(dict.GetMode(&h.mode));
  EXPECT_EQ(ProxyPrefs::MODE_SYSTEM, h.mode);
  ASSERT_FALSE(dict.GetPacUrl(&h.bypass_list));
  ASSERT_FALSE(dict.GetProxyServer(&h.proxy_server));
  ASSERT_FALSE(dict.GetBypassList(&h.bypass_list));
}
