// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/package_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using PackageIdTest = testing::Test;

TEST_F(PackageIdTest, FromStringValidWeb) {
  std::optional<PackageId> id =
      PackageId::FromString("web:https://www.app.com/");

  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->package_type(), PackageType::kWeb);
  ASSERT_EQ(id->identifier(), "https://www.app.com/");
}

TEST_F(PackageIdTest, FromStringValidAndroid) {
  std::optional<PackageId> id =
      PackageId::FromString("android:com.google.android.apps.photos");

  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->package_type(), PackageType::kArc);
  ASSERT_EQ(id->identifier(), "com.google.android.apps.photos");
}

TEST_F(PackageIdTest, FromStringValidChromeApp) {
  std::optional<PackageId> id =
      PackageId::FromString("chromeapp:mmfbcljfglbokpmkimbfghdkjmjhdgbg");

  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->package_type(), PackageType::kChromeApp);
  ASSERT_EQ(id->identifier(), "mmfbcljfglbokpmkimbfghdkjmjhdgbg");
}

TEST_F(PackageIdTest, FromStringValidWebsite) {
  std::optional<PackageId> id =
      PackageId::FromString("website:https://www.example.com/");

  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->package_type(), PackageType::kWebsite);
  ASSERT_EQ(id->identifier(), "https://www.example.com/");
}

TEST_F(PackageIdTest, FromStringInvalidFormat) {
  ASSERT_FALSE(PackageId::FromString("foobar").has_value());
  ASSERT_FALSE(PackageId::FromString("web:").has_value());
  ASSERT_FALSE(PackageId::FromString("").has_value());
  ASSERT_FALSE(PackageId::FromString(":").has_value());
}

TEST_F(PackageIdTest, FromStringInvalidType) {
  std::optional<PackageId> id = PackageId::FromString("coolplatform:myapp");

  ASSERT_FALSE(id.has_value());
}

TEST_F(PackageIdTest, FromStringUnknownType) {
  std::optional<PackageId> id = PackageId::FromString("unknown:foo");

  ASSERT_FALSE(id.has_value());
}

TEST_F(PackageIdTest, ToStringWeb) {
  PackageId id(PackageType::kWeb, "https://www.app.com/");

  ASSERT_EQ(id.ToString(), "web:https://www.app.com/");
}

TEST_F(PackageIdTest, ToStringAndroid) {
  PackageId id(PackageType::kArc, "com.google.android.apps.photos");

  ASSERT_EQ(id.ToString(), "android:com.google.android.apps.photos");
}

TEST_F(PackageIdTest, ToStringChromeApp) {
  PackageId id(PackageType::kChromeApp, "mmfbcljfglbokpmkimbfghdkjmjhdgbg");

  ASSERT_EQ(id.ToString(), "chromeapp:mmfbcljfglbokpmkimbfghdkjmjhdgbg");
}

TEST_F(PackageIdTest, ToStringWebsite) {
  PackageId id(PackageType::kWebsite, "https://www.example.com/");

  ASSERT_EQ(id.ToString(), "website:https://www.example.com/");
}

TEST_F(PackageIdTest, ToStringUnknown) {
  PackageId id(PackageType::kUnknown, "someapp");

  ASSERT_EQ(id.ToString(), "unknown:someapp");
}

}  // namespace apps
