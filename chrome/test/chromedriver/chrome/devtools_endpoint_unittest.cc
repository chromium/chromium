// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_endpoint.h"
#include "chrome/test/chromedriver/net/net_util.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(DevToolsEndpoint, Invalid) {
  const DevToolsEndpoint endpoint;
  ASSERT_FALSE(endpoint.IsValid());
}

TEST(DevToolsEndpoint, FromPort) {
  const DevToolsEndpoint endpoint(9999);
  ASSERT_TRUE(endpoint.IsValid());
  ASSERT_EQ(endpoint.Address().ToString(), NetAddress(9999).ToString());
  ASSERT_EQ(endpoint.GetBrowserDebuggerUrl(),
            "ws://localhost:9999/devtools/browser/");
  ASSERT_EQ(endpoint.GetDebuggerUrl("abc"),
            "ws://localhost:9999/devtools/page/abc");
  ASSERT_EQ(endpoint.GetVersionUrl(), "http://localhost:9999/json/version");
  ASSERT_EQ(endpoint.GetListUrl(), "http://localhost:9999/json/list");
  ASSERT_EQ(endpoint.GetCloseUrl("xyz"),
            "http://localhost:9999/json/close/xyz");
  ASSERT_EQ(endpoint.GetActivateUrl("xyz"),
            "http://localhost:9999/json/activate/xyz");
}

TEST(DevToolsEndpoint, FromNetAddress) {
  const DevToolsEndpoint endpoint(NetAddress(9222));
  ASSERT_TRUE(endpoint.IsValid());
  ASSERT_EQ(endpoint.Address().ToString(), NetAddress(9222).ToString());
  ASSERT_EQ(endpoint.GetBrowserDebuggerUrl(),
            "ws://localhost:9222/devtools/browser/");
  ASSERT_EQ(endpoint.GetDebuggerUrl("abc"),
            "ws://localhost:9222/devtools/page/abc");
  ASSERT_EQ(endpoint.GetVersionUrl(), "http://localhost:9222/json/version");
  ASSERT_EQ(endpoint.GetListUrl(), "http://localhost:9222/json/list");
  ASSERT_EQ(endpoint.GetCloseUrl("xyz"),
            "http://localhost:9222/json/close/xyz");
  ASSERT_EQ(endpoint.GetActivateUrl("xyz"),
            "http://localhost:9222/json/activate/xyz");
}

TEST(DevToolsEndpoint, FromHttpUrl) {
  const DevToolsEndpoint endpoint("http://remote:9223/custom/path/");
  ASSERT_TRUE(endpoint.IsValid());
  ASSERT_EQ(endpoint.Address().ToString(),
            NetAddress("remote", 9223).ToString());
  ASSERT_EQ(endpoint.GetBrowserDebuggerUrl(),
            "ws://remote:9223/custom/path/devtools/browser/");
  ASSERT_EQ(endpoint.GetDebuggerUrl("abc"),
            "ws://remote:9223/custom/path/devtools/page/abc");
  ASSERT_EQ(endpoint.GetVersionUrl(),
            "http://remote:9223/custom/path/json/version");
  ASSERT_EQ(endpoint.GetListUrl(), "http://remote:9223/custom/path/json/list");
  ASSERT_EQ(endpoint.GetCloseUrl("xyz"),
            "http://remote:9223/custom/path/json/close/xyz");
  ASSERT_EQ(endpoint.GetActivateUrl("xyz"),
            "http://remote:9223/custom/path/json/activate/xyz");
}

TEST(DevToolsEndpoint, FromHttpsUrl) {
  const DevToolsEndpoint endpoint("https://secure:9224/custom/path/");
  ASSERT_TRUE(endpoint.IsValid());
  ASSERT_EQ(endpoint.Address().ToString(),
            NetAddress("secure", 9224).ToString());
  ASSERT_EQ(endpoint.GetBrowserDebuggerUrl(),
            "wss://secure:9224/custom/path/devtools/browser/");
  ASSERT_EQ(endpoint.GetDebuggerUrl("abc"),
            "wss://secure:9224/custom/path/devtools/page/abc");
  ASSERT_EQ(endpoint.GetVersionUrl(),
            "https://secure:9224/custom/path/json/version");
  ASSERT_EQ(endpoint.GetListUrl(), "https://secure:9224/custom/path/json/list");
  ASSERT_EQ(endpoint.GetCloseUrl("xyz"),
            "https://secure:9224/custom/path/json/close/xyz");
  ASSERT_EQ(endpoint.GetActivateUrl("xyz"),
            "https://secure:9224/custom/path/json/activate/xyz");
}
