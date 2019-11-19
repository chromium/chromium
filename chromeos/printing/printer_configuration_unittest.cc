// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri_components.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(PrinterConfigurationTest, EmptyScheme) {
  auto result = chromeos::ParseUri("://hostname.com/");
  EXPECT_FALSE(result.has_value());
}

TEST(PrinterConfigurationTest, JustScheme) {
  auto result = chromeos::ParseUri("ipps://");
  EXPECT_FALSE(result.has_value());
}

TEST(PrinterConfigurationTest, InvalidUriDanglingPort) {
  auto result = chromeos::ParseUri("ipp://192.168.1.1:");
  EXPECT_FALSE(result.has_value());
}

TEST(PrinterConfigurationTest, InvalidUriEmptyPort) {
  auto result = chromeos::ParseUri("ipp://192.168.1.1:/printer");
  EXPECT_FALSE(result.has_value());
}

TEST(PrinterConfigurationTest, InvalidPort) {
  auto result = chromeos::ParseUri("ipp://1.2.3.4:abcd");
  EXPECT_FALSE(result.has_value());
}

TEST(PrinterConfigurationTest, MissingScheme) {
  auto result = chromeos::ParseUri("/10.2.12.3/");
  EXPECT_FALSE(result.has_value());
}

TEST(PrinterConfigurationTest, ParseUriIp) {
  auto result = chromeos::ParseUri("ipp://192.168.1.5");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->scheme(), "ipp");
  EXPECT_EQ(result->host(), "192.168.1.5");
  EXPECT_TRUE(result->path().empty());
}

TEST(PrinterConfigurationTest, ParseUriPort) {
  auto result = chromeos::ParseUri("ipp://1.2.3.4:4444");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->port(), 4444);
}

TEST(PrinterConfigurationTest, ParseTrailingSlash) {
  auto result = chromeos::ParseUri("ipp://1.2.3.4/");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->path(), "/");
  EXPECT_EQ(result->host(), "1.2.3.4");
}

TEST(PrinterConfigurationTest, ParseUriHostNameAndPort) {
  auto result = chromeos::ParseUri("ipp://chromium.org:8");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->port(), 8);
  EXPECT_EQ(result->host(), "chromium.org");
}

TEST(PrinterConfigurationTest, ParseUriPathNoPort) {
  auto result = chromeos::ParseUri("ipps://chromium.org/printers/printprint");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->host(), "chromium.org");
  EXPECT_EQ(result->path(), "/printers/printprint");
}

TEST(PrinterConfigurationTest, ParseUriSubdomainQueueAndPort) {
  auto result =
      chromeos::ParseUri("ipp://codesearch.chromium.org:1234/ipp/print");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->host(), "codesearch.chromium.org");
  EXPECT_EQ(result->port(), 1234);
  EXPECT_EQ(result->path(), "/ipp/print");
}

}  // namespace
