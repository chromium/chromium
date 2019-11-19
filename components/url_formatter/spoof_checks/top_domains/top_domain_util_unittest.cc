// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"

#include "testing/gtest/include/gtest/gtest.h"

using url_formatter::top_domains::HostnameWithoutRegistry;
using url_formatter::top_domains::IsEditDistanceCandidate;

TEST(TopDomainUtilTest, IsEditDistanceCandidate) {
  EXPECT_FALSE(IsEditDistanceCandidate(""));
  EXPECT_TRUE(IsEditDistanceCandidate("google.com"));
  // Domain label ("abc") is too short, even though the whole string is long
  // enough.
  EXPECT_FALSE(IsEditDistanceCandidate("abc.com.tr"));
}

TEST(TopDomainUtilTest, HostnameWithoutRegistry) {
  EXPECT_EQ("google", HostnameWithoutRegistry("google"));
  EXPECT_EQ("google.", HostnameWithoutRegistry("google."));
  EXPECT_EQ("google..", HostnameWithoutRegistry("google.."));
  EXPECT_EQ("google.", HostnameWithoutRegistry("google.com"));
  EXPECT_EQ("google.", HostnameWithoutRegistry("google.com.tr"));
  // blogspot.com is a private registry.
  EXPECT_EQ("blogspot.", HostnameWithoutRegistry("blogspot.com"));
}
