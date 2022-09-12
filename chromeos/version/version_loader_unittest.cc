// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/version/version_loader.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test VersionLoaderTest;

static const char kTest10[] = "vendor            | FOO";
static const char kTest11[] = "firmware          | FOO";
static const char kTest12[] = "firmware          | FOO";
static const char kTest13[] = "version           | 0.2.3.3";
static const char kTest14[] = "version        | 0.2.3.3";
static const char kTest15[] = "version             0.2.3.3";

TEST_F(VersionLoaderTest, ParseFirmware) {
  EXPECT_EQ("", version_loader::ParseFirmware(kTest10));
  EXPECT_EQ("", version_loader::ParseFirmware(kTest11));
  EXPECT_EQ("", version_loader::ParseFirmware(kTest12));
  EXPECT_EQ("0.2.3.3", version_loader::ParseFirmware(kTest13));
  EXPECT_EQ("0.2.3.3", version_loader::ParseFirmware(kTest14));
  EXPECT_EQ("0.2.3.3", version_loader::ParseFirmware(kTest15));
}

TEST_F(VersionLoaderTest, IsRollback) {
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "1.2.3.4"));
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "1.2.3.5"));
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "1.2.4.5"));
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "1.3.0.0"));
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "2.0.0.0"));
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "2.3.4.5"));
  EXPECT_FALSE(version_loader::IsRollback("1.0.0.0", "2.0.0.0"));
  EXPECT_TRUE(version_loader::IsRollback("1.2.3.4", "1.2.3.3"));
  EXPECT_TRUE(version_loader::IsRollback("1.2.3.4", "1.1.1.1"));
  EXPECT_TRUE(version_loader::IsRollback("1.0.0.0", "0.9.0.0"));

  // If possible, use number comparison, otherwise string comparison.
  EXPECT_FALSE(version_loader::IsRollback("999.0.0.0", "1000.0.0.0"));
  EXPECT_TRUE(version_loader::IsRollback("1000.0.0.0", "999.0.0.0"));
  EXPECT_FALSE(version_loader::IsRollback("1000x.0.0.0", "999x.0.0.0"));
  EXPECT_TRUE(version_loader::IsRollback("999x.0.0.0", "1000x.0.0.0"));

  EXPECT_FALSE(version_loader::IsRollback("1.0", "1.1"));
  EXPECT_FALSE(version_loader::IsRollback("1.0", "1.0.1"));
  EXPECT_FALSE(version_loader::IsRollback("1", "1.1"));
  EXPECT_TRUE(version_loader::IsRollback("1.0", "0.9"));
  EXPECT_TRUE(version_loader::IsRollback("3", "2.9"));
  EXPECT_TRUE(version_loader::IsRollback("3.1", "3"));
  EXPECT_TRUE(version_loader::IsRollback("3.0.1", "3"));
  EXPECT_TRUE(version_loader::IsRollback("1.0", "0.0"));

  EXPECT_FALSE(version_loader::IsRollback("", ""));
  EXPECT_FALSE(version_loader::IsRollback("invalid", "invalid"));
  EXPECT_FALSE(version_loader::IsRollback("alpha", "beta"));
  EXPECT_TRUE(version_loader::IsRollback("beta", "alpha"));
  EXPECT_FALSE(version_loader::IsRollback("10.alpha", "10.beta"));
  EXPECT_TRUE(version_loader::IsRollback("10.beta", "10.alpha"));
  EXPECT_FALSE(version_loader::IsRollback("10.beta", "11.alpha"));
  EXPECT_TRUE(version_loader::IsRollback("11.alpha", "10.beta"));

  // 0.0.0.0 means update is not available, it is not a rollback.
  EXPECT_FALSE(version_loader::IsRollback("1.2.3.4", "0.0.0.0"));
  EXPECT_FALSE(version_loader::IsRollback("1.0", "0.0.0.0"));
  EXPECT_FALSE(version_loader::IsRollback("0.0.0.0", "0.0.0.0"));

  // If there are string parts, there are compared as strings.
  EXPECT_FALSE(version_loader::IsRollback("1.2.2018_01_01", "1.2.2018_01_02"));
  EXPECT_FALSE(version_loader::IsRollback("1.2.2018_01_01", "1.2.2018_01_01"));
  EXPECT_TRUE(version_loader::IsRollback("1.2.2018_01_02", "1.2.2018_01_01"));
  EXPECT_FALSE(version_loader::IsRollback("1.2018_01_01.2", "1.2018_01_01.3"));
  EXPECT_FALSE(version_loader::IsRollback("1.2018_01_01.2", "1.2018_01_01.2"));
  EXPECT_TRUE(version_loader::IsRollback("1.2018_01_01.2", "1.2018_01_01.1"));
}

}  // namespace chromeos
