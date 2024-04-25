// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_package {

TEST(WebBundleUtilsTest, IsValidUuidInPackageURL) {
  ASSERT_TRUE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:f81d4fae-7dec-11d0-a765-00a0c91e6bf6")));
  ASSERT_TRUE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-0000-000000000000")));
  ASSERT_FALSE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-0000-000000000000-0")));
  ASSERT_FALSE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-0000-00000000000")));
  ASSERT_FALSE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-0000-00000000000g")));
  ASSERT_FALSE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-00000000-00000000")));
  ASSERT_FALSE(IsValidUuidInPackageURL(
      GURL("urn:guid:00000000-0000-0000-0000-000000000000")));
  ASSERT_FALSE(IsValidUuidInPackageURL(
      GURL("uri:uuid:00000000-0000-0000-0000-000000000000")));
  ASSERT_TRUE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:F81D4FAE-7DEC-11D0-A765-00A0C91E6BF6")));
  ASSERT_TRUE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-0000-000000000000")));
  ASSERT_TRUE(IsValidUuidInPackageURL(
      GURL("uuid-in-package:00000000-0000-0000-0000-000000000000")));
}

}  // namespace web_package
