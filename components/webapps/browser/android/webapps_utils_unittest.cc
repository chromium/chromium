// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapps_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace webapps {

namespace {

blink::Manifest GetValidManifest() {
  blink::Manifest manifest;
  manifest.name = base::ASCIIToUTF16("foo");
  manifest.short_name = base::ASCIIToUTF16("bar");
  manifest.start_url = GURL("http://example.com");
  manifest.display = blink::mojom::DisplayMode::kStandalone;

  blink::Manifest::ImageResource icon;
  icon.type = base::ASCIIToUTF16("image/png");
  icon.sizes.push_back(gfx::Size(144, 144));
  manifest.icons.push_back(icon);

  return manifest;
}

}  // anonymous namespace

TEST(WebappsUtilsTest, Compatible) {
  blink::Manifest manifest = GetValidManifest();
  EXPECT_TRUE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest));
}

TEST(WebappsUtilsTest, CompatibleURLHasNoPassword) {
  const GURL kUrlWithPassword("http://answer:42@life/universe/and/everything");

  blink::Manifest manifest = GetValidManifest();
  manifest.start_url = kUrlWithPassword;
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest));

  manifest = GetValidManifest();
  manifest.scope = kUrlWithPassword;
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest));

  manifest = GetValidManifest();
  manifest.icons[0].src = kUrlWithPassword;
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest));
}

}  // namespace webapps
