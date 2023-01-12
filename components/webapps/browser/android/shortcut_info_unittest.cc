// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/shortcut_info.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace webapps {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

blink::Manifest::ImageResource CreateImage(const std::string& url,
                                           const gfx::Size& size,
                                           Purpose purpose) {
  blink::Manifest::ImageResource image;
  image.src = GURL("https://example.com" + url);
  image.sizes.push_back(size);
  image.purpose.push_back(purpose);
  return image;
}

blink::Manifest::ShortcutItem CreateShortcut(
    const std::string& name,
    const std::vector<blink::Manifest::ImageResource>& icons) {
  blink::Manifest::ShortcutItem shortcut;
  shortcut.name = base::UTF8ToUTF16(name);
  shortcut.url = GURL("https://example.com/");
  shortcut.icons = icons;
  return shortcut;
}

class ShortcutInfoTest : public testing::Test {
 public:
  ShortcutInfoTest() : info_(GURL()) {}

  ShortcutInfoTest(const ShortcutInfoTest&) = delete;
  ShortcutInfoTest& operator=(const ShortcutInfoTest&) = delete;

 protected:
  ShortcutInfo info_;
  blink::mojom::Manifest manifest_;
};

TEST_F(ShortcutInfoTest, AllAttributesUpdate) {
  info_.name = u"old name";
  manifest_.name = u"new name";

  info_.short_name = u"old short name";
  manifest_.short_name = u"new short name";

  info_.url = GURL("https://old.com/start");
  manifest_.start_url = GURL("https://new.com/start");

  info_.scope = GURL("https://old.com/");
  manifest_.scope = GURL("https://new.com/");

  info_.display = blink::mojom::DisplayMode::kStandalone;
  manifest_.display = blink::mojom::DisplayMode::kFullscreen;

  info_.theme_color = 0xffff0000;
  manifest_.has_theme_color = true;
  manifest_.theme_color = 0xffcc0000;

  info_.background_color = 0xffaa0000;
  manifest_.has_background_color = true;
  manifest_.background_color = 0xffbb0000;

  info_.icon_urls.push_back("https://old.com/icon.png");
  blink::Manifest::ImageResource icon;
  icon.src = GURL("https://new.com/icon.png");
  manifest_.icons.push_back(icon);

  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name, info_.name);
  ASSERT_EQ(manifest_.short_name, info_.short_name);
  ASSERT_EQ(manifest_.start_url, info_.url);
  ASSERT_EQ(manifest_.scope, info_.scope);
  ASSERT_EQ(manifest_.display, info_.display);
  ASSERT_EQ(manifest_.theme_color, info_.theme_color);
  ASSERT_EQ(manifest_.background_color, info_.background_color);
  ASSERT_EQ(1u, info_.icon_urls.size());
  ASSERT_EQ(manifest_.icons[0].src, GURL(info_.icon_urls[0]));
}

TEST_F(ShortcutInfoTest, GetAllWebApkIcons) {
  GURL best_primary_icon_url("https://best_primary_icon.png");
  GURL splash_image_url("https://splash_image.png");
  GURL best_shortcut_icon_url_1("https://best_shortcut_icon_1.png");
  GURL best_shortcut_icon_url_2("https://best_shortcut_icon_2.png");

  info_.best_shortcut_icon_urls.push_back(best_shortcut_icon_url_1);
  info_.best_shortcut_icon_urls.push_back(best_shortcut_icon_url_2);
  info_.best_primary_icon_url = best_primary_icon_url;
  info_.splash_image_url = splash_image_url;

  std::set<GURL> result = info_.GetWebApkIcons();
  std::set<GURL> expected{best_shortcut_icon_url_1, best_shortcut_icon_url_2,
                          best_primary_icon_url, splash_image_url};

  ASSERT_EQ(4u, result.size());
  ASSERT_EQ(expected, result);
}

TEST_F(ShortcutInfoTest, NotContainEmptyOrDuplicateWebApkIcons) {
  GURL best_primary_icon_url = GURL("https://best_primary_icon.com");
  GURL best_shortcut_icon_url = GURL("https://best_shortcut_icon_1.com");

  info_.best_shortcut_icon_urls.push_back(best_shortcut_icon_url);
  info_.best_shortcut_icon_urls.push_back(best_primary_icon_url);
  info_.best_primary_icon_url = best_primary_icon_url;

  std::set<GURL> result = info_.GetWebApkIcons();
  std::set<GURL> expected{best_shortcut_icon_url, best_primary_icon_url};

  ASSERT_EQ(2u, result.size());
  ASSERT_EQ(expected, result);
}

TEST_F(ShortcutInfoTest, NameFallsBackToShortName) {
  manifest_.short_name = u"short_name";
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name, info_.name);
}

TEST_F(ShortcutInfoTest, ShortNameFallsBackToName) {
  manifest_.name = u"name";
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name, info_.short_name);
}

TEST_F(ShortcutInfoTest, UserTitleBecomesShortName) {
  manifest_.short_name = u"name";
  info_.user_title = u"title";
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name, info_.user_title);
}

// Test that if a manifest with an empty name and empty short_name is passed,
// that ShortcutInfo::UpdateFromManifest() does not overwrite the current
// ShortcutInfo::name and ShortcutInfo::short_name.
TEST_F(ShortcutInfoTest, IgnoreEmptyNameAndShortName) {
  std::u16string initial_name(u"initial_name");
  std::u16string initial_short_name(u"initial_short_name");

  info_.name = initial_name;
  info_.short_name = initial_short_name;
  manifest_.display = blink::mojom::DisplayMode::kStandalone;
  manifest_.name = std::u16string();
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(initial_name, info_.name);
  ASSERT_EQ(initial_short_name, info_.short_name);
}

TEST_F(ShortcutInfoTest, ShortcutItemsPopulated) {
  manifest_.shortcuts.push_back(CreateShortcut(
      "shortcut_1",
      {CreateImage("/i1_1", {16, 16}, Purpose::ANY),
       CreateImage("/i1_2", {64, 64}, Purpose::ANY),
       CreateImage("/i1_3", {192, 192}, Purpose::ANY),  // best icon.
       CreateImage("/i1_4", {256, 256}, Purpose::ANY)}));

  manifest_.shortcuts.push_back(CreateShortcut(
      "shortcut_2",
      {CreateImage("/i2_1", {192, 194}, Purpose::ANY),  // not square.
       CreateImage("/i2_2", {194, 194}, Purpose::ANY)}));

  // Nothing chosen.
  manifest_.shortcuts.push_back(CreateShortcut(
      "shortcut_3", {CreateImage("/i3_1", {16, 16}, Purpose::ANY)}));

  WebappsIconUtils::SetIdealShortcutSizeForTesting(192);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(info_.best_shortcut_icon_urls.size(), 3u);
  EXPECT_EQ(info_.best_shortcut_icon_urls[0].path(), "/i1_3");
  EXPECT_EQ(info_.best_shortcut_icon_urls[1].path(), "/i2_2");
  EXPECT_FALSE(info_.best_shortcut_icon_urls[2].is_valid());
}

// Tests that if the optional shortcut short_name value is not provided, the
// required name value is used.
TEST_F(ShortcutInfoTest, ShortcutShortNameBackfilled) {
  // Create a shortcut without a |short_name|.
  manifest_.shortcuts.push_back(
      CreateShortcut(/* name= */ "name", /* icons= */ {}));

  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(info_.shortcut_items.size(), 1u);
  EXPECT_EQ(info_.shortcut_items[0].short_name, u"name");
}

TEST_F(ShortcutInfoTest, FindMaskableSplashIcon) {
  manifest_.icons.push_back(
      CreateImage("/icon_48.png", {48, 48}, Purpose::ANY));
  manifest_.icons.push_back(
      CreateImage("/icon_96.png", {96, 96}, Purpose::ANY));
  manifest_.icons.push_back(
      CreateImage("/icon_144.png", {144, 144}, Purpose::MASKABLE));

  info_.UpdateBestSplashIcon(manifest_);

  if (WebappsIconUtils::DoesAndroidSupportMaskableIcons()) {
    EXPECT_EQ(info_.splash_image_url.path(), "/icon_144.png");
    EXPECT_TRUE(info_.is_splash_image_maskable);
  } else {
    EXPECT_EQ(info_.splash_image_url.path(), "/icon_96.png");
    EXPECT_FALSE(info_.is_splash_image_maskable);
  }
}

TEST_F(ShortcutInfoTest, SplashIconFallbackToAny) {
  manifest_.icons.push_back(
      CreateImage("/icon_48.png", {48, 48}, Purpose::ANY));
  manifest_.icons.push_back(
      CreateImage("/icon_96.png", {96, 96}, Purpose::ANY));
  manifest_.icons.push_back(
      CreateImage("/icon_144.png", {144, 144}, Purpose::ANY));

  info_.UpdateBestSplashIcon(manifest_);

  EXPECT_EQ(info_.splash_image_url.path(), "/icon_144.png");
  EXPECT_FALSE(info_.is_splash_image_maskable);
}

TEST_F(ShortcutInfoTest, DisplayOverride) {
  manifest_.display = blink::mojom::DisplayMode::kBrowser;
  manifest_.display_override = {blink::mojom::DisplayMode::kMinimalUi};
  info_.UpdateFromManifest(manifest_);
  EXPECT_EQ(info_.display, blink::mojom::DisplayMode::kMinimalUi);

  manifest_.display = blink::mojom::DisplayMode::kFullscreen;
  manifest_.display_override = {blink::mojom::DisplayMode::kBrowser};
  info_.UpdateFromManifest(manifest_);
  EXPECT_EQ(info_.display, blink::mojom::DisplayMode::kBrowser);

  manifest_.display = blink::mojom::DisplayMode::kStandalone;
  manifest_.display_override = {blink::mojom::DisplayMode::kFullscreen};
  info_.UpdateFromManifest(manifest_);
  EXPECT_EQ(info_.display, blink::mojom::DisplayMode::kFullscreen);

  manifest_.display = blink::mojom::DisplayMode::kMinimalUi;
  manifest_.display_override = {blink::mojom::DisplayMode::kStandalone};
  info_.UpdateFromManifest(manifest_);
  EXPECT_EQ(info_.display, blink::mojom::DisplayMode::kStandalone);
}

TEST_F(ShortcutInfoTest, ManifestIdGenerated) {
  manifest_.start_url = GURL("https://new.com/start");
  manifest_.id = u"new_id";

  info_.UpdateFromManifest(manifest_);

  EXPECT_EQ(info_.manifest_id.spec(), "https://new.com/new_id");
}

TEST_F(ShortcutInfoTest, ManifestIdFallback) {
  manifest_.start_url = GURL("https://new.com/start");
  manifest_.id = absl::nullopt;

  info_.UpdateFromManifest(manifest_);

  // If id is not specified, use start_url.
  EXPECT_EQ(info_.manifest_id.spec(), manifest_.start_url.spec());
}

}  // namespace webapps
