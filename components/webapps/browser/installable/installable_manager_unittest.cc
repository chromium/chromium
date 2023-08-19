// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_manager.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

class InstallableManagerUnitTest : public testing::Test {
 public:
  InstallableManagerUnitTest()
      : manager_(std::make_unique<InstallableManager>(nullptr)) {}

 protected:
  static blink::mojom::ManifestPtr GetValidManifest() {
    auto manifest = blink::mojom::Manifest::New();
    manifest->name = u"foo";
    manifest->short_name = u"bar";
    manifest->start_url = GURL("http://example.com");
    manifest->id = manifest->start_url;
    manifest->display = blink::mojom::DisplayMode::kStandalone;

    blink::Manifest::ImageResource primary_icon;
    primary_icon.type = u"image/png";
    primary_icon.sizes.emplace_back(144, 144);
    primary_icon.purpose.push_back(IconPurpose::ANY);
    manifest->icons.push_back(primary_icon);

    // No need to include the optional badge icon as it does not affect the
    // unit tests.
    return manifest;
  }

  bool IsManifestValid(const blink::mojom::Manifest& manifest,
                       bool check_webapp_manifest_display = true) {
    // Explicitly reset the error code before running the method.
    manager_->set_valid_manifest_error(NO_ERROR_DETECTED);
    return manager_->IsManifestValidForWebApp(manifest,
                                              check_webapp_manifest_display);
  }

  InstallableStatusCode GetErrorCode() {
    return manager_->valid_manifest_error();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<InstallableManager> manager_;
};

TEST_F(InstallableManagerUnitTest, EmptyManifestIsInvalid) {
  blink::mojom::Manifest manifest;
  EXPECT_FALSE(IsManifestValid(manifest));
  EXPECT_EQ(MANIFEST_EMPTY, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, CheckMinimalValidManifest) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestRequiresNameOrShortName) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->name = absl::nullopt;
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->name = u"foo";
  manifest->short_name = absl::nullopt;
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->name = absl::nullopt;
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_NAME_OR_SHORT_NAME, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestRequiresNonEmptyNameORShortName) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->name = std::u16string();
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->name = u"foo";
  manifest->short_name = std::u16string();
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->name = std::u16string();
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_NAME_OR_SHORT_NAME, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestRequiresValidStartURL) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->start_url = GURL();
  manifest->id = GURL();
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(START_URL_NOT_VALID, GetErrorCode());

  manifest->start_url = GURL("/");
  manifest->id = GURL("/");
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(START_URL_NOT_VALID, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestSupportsImagePNG) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->icons[0].type = u"image/gif";
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  manifest->icons[0].type.clear();
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If the type is null, the icon src will be checked instead.
  manifest->icons[0].src = GURL("http://example.com/icon.png");
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Capital file extension is also permissible.
  manifest->icons[0].src = GURL("http://example.com/icon.PNG");
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Unsupported extensions are rejected.
  manifest->icons[0].src = GURL("http://example.com/icon.gif");
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestSupportsImageSVG) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  // The correct mimetype is image/svg+xml.
  manifest->icons[0].type = u"image/svg";
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If the type is null, the icon src will be checked instead.
  manifest->icons[0].type.clear();
  manifest->icons[0].src = GURL("http://example.com/icon.svg");
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Capital file extension is also permissible.
  manifest->icons[0].src = GURL("http://example.com/icon.SVG");
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestSupportsImageWebP) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->icons[0].type = u"image/webp";
  manifest->icons[0].src = GURL("http://example.com/");
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // If the type is null, the icon src is checked instead.
  // Case is ignored.
  manifest->icons[0].type.clear();
  manifest->icons[0].src = GURL("http://example.com/icon.wEBp");
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestRequiresPurposeAny) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  // The icon MUST have IconPurpose::ANY at least.
  manifest->icons[0].purpose[0] = IconPurpose::MASKABLE;
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If one of the icon purposes match the requirement, it should be accepted.
  manifest->icons[0].purpose.push_back(IconPurpose::ANY);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestRequiresIconSize) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  // The icon MUST be 144x144 size at least.
  manifest->icons[0].sizes[0] = gfx::Size(1, 1);
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  manifest->icons[0].sizes[0] = gfx::Size(143, 143);
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If one of the sizes match the requirement, it should be accepted.
  manifest->icons[0].sizes.emplace_back(144, 144);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Higher than the required size is okay.
  manifest->icons[0].sizes[1] = gfx::Size(200, 200);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Icon size matching the maximum size requirement is correct.
  manifest->icons[0].sizes[1] = gfx::Size(1024, 1024);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Icon size larger than maximum size 1024x1024 should not
  // be accepted on desktop.
  manifest->icons[0].sizes[1] = gfx::Size(1025, 1025);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
#else
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());
#endif  // BUILDFLAG(IS_ANDROID)

  // Non-square is okay.
  manifest->icons[0].sizes[1] = gfx::Size(144, 200);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // The representation of the keyword 'any' should be recognized.
  manifest->icons[0].sizes[1] = gfx::Size(0, 0);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestDisplayModes) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->display = blink::mojom::DisplayMode::kUndefined;
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED, GetErrorCode());

  manifest->display = blink::mojom::DisplayMode::kBrowser;
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED, GetErrorCode());

  manifest->display = blink::mojom::DisplayMode::kMinimalUi;
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display = blink::mojom::DisplayMode::kStandalone;
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display = blink::mojom::DisplayMode::kFullscreen;
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display = blink::mojom::DisplayMode::kWindowControlsOverlay;
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display = blink::mojom::DisplayMode::kTabbed;
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, ManifestDisplayOverride) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->display_override.push_back(blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display_override.push_back(blink::mojom::DisplayMode::kBrowser);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display_override.insert(manifest->display_override.begin(),
                                    blink::mojom::DisplayMode::kStandalone);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display_override.insert(manifest->display_override.begin(),
                                    blink::mojom::DisplayMode::kStandalone);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display_override.insert(manifest->display_override.begin(),
                                    blink::mojom::DisplayMode::kBrowser);
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED, GetErrorCode());

  manifest->display_override.insert(
      manifest->display_override.begin(),
      blink::mojom::DisplayMode::kWindowControlsOverlay);
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest->display_override.insert(manifest->display_override.begin(),
                                    blink::mojom::DisplayMode::kTabbed);
  EXPECT_TRUE(
      IsManifestValid(*manifest, false /* check_webapp_manifest_display */));
  EXPECT_FALSE(IsManifestValid(*manifest));
  EXPECT_EQ(MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, FallbackToBrowser) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->display = blink::mojom::DisplayMode::kBrowser;
  manifest->display_override.push_back(blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableManagerUnitTest, SupportWindowControlsOverlay) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->display_override.push_back(
      blink::mojom::DisplayMode::kWindowControlsOverlay);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

class InstallableManagerUnitTest_Tabbed : public InstallableManagerUnitTest {
 public:
  InstallableManagerUnitTest_Tabbed() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDesktopPWAsTabStrip};
};

TEST_F(InstallableManagerUnitTest_Tabbed, SupportTabbed) {
  blink::mojom::ManifestPtr manifest = GetValidManifest();

  manifest->display_override.push_back(blink::mojom::DisplayMode::kTabbed);
  EXPECT_TRUE(IsManifestValid(*manifest));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

}  // namespace webapps
