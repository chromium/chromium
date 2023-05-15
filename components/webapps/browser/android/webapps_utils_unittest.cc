// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapps_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace webapps {

namespace {

blink::mojom::ManifestPtr GetValidManifest() {
  auto manifest = blink::mojom::Manifest::New();
  manifest->name = u"foo";
  manifest->short_name = u"bar";
  manifest->start_url = GURL("http://example.com");
  manifest->id = manifest->start_url;
  manifest->display = blink::mojom::DisplayMode::kStandalone;

  blink::Manifest::ImageResource icon;
  icon.type = u"image/png";
  icon.sizes.push_back(gfx::Size(144, 144));
  manifest->icons.push_back(icon);

  return manifest;
}

}  // anonymous namespace

TEST(WebappsUtilsTest, Compatible) {
  EXPECT_TRUE(
      WebappsUtils::AreWebManifestUrlsWebApkCompatible(*GetValidManifest()));
}

TEST(WebappsUtilsTest, CompatibleURLHasNoPassword) {
  const GURL kUrlWithPassword("http://answer:42@life/universe/and/everything");

  blink::mojom::ManifestPtr manifest = GetValidManifest();
  manifest->start_url = kUrlWithPassword;
  manifest->id = kUrlWithPassword;
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(*manifest));

  manifest = GetValidManifest();
  manifest->scope = kUrlWithPassword;
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(*manifest));

  manifest = GetValidManifest();
  manifest->icons[0].src = kUrlWithPassword;
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(*manifest));
}

}  // namespace webapps
