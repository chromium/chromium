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
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

class InstallableEvaluatorUnitTest : public content::RenderViewHostTestHarness {
 public:
  InstallableEvaluatorUnitTest()
      : page_data_(std::make_unique<InstallablePageData>()) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

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

  static mojom::WebPageMetadataPtr GetWebPageMetadata() {
    auto metadata = mojom::WebPageMetadata::New();
    metadata->application_name = u"foo";
    metadata->application_url = GURL("http://example.com");
    mojom::WebPageIconInfoPtr icon_info(mojom::WebPageIconInfo::New());
    metadata->icons.push_back(std::move(icon_info));

    return metadata;
  }

  void AddFavicon() {
    const auto favicon_url = blink::mojom::FaviconURL::New(
        GURL{"http://www.google.com/favicon.ico"},
        blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
        /*is_default_icon=*/false);

    std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
    favicon_urls.push_back(mojo::Clone(favicon_url));

    web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));
  }

  bool IsWebAppInstallable(InstallableCriteria criteria) {
    evaluator_ = std::make_unique<InstallableEvaluator>(web_contents(),
                                                        *page_data_, criteria);

    errors_ = evaluator_->CheckInstallability().value();
    return errors_.empty();
  }

  InstallableStatusCode GetErrorCode() {
    return errors_.empty() ? NO_ERROR_DETECTED : errors_[0];
  }

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    page_data_->manifest_->manifest = std::move(manifest);
  }

  void SetMetadata(mojom::WebPageMetadataPtr metadata) {
    page_data_->web_page_metadata_->metadata = (std::move(metadata));
  }

  blink::mojom::Manifest* manifest() {
    return page_data_->manifest_->manifest.get();
  }

  mojom::WebPageMetadata* metadata() {
    return page_data_->web_page_metadata_->metadata.get();
  }

 private:
  std::unique_ptr<InstallablePageData> page_data_;
  std::unique_ptr<InstallableEvaluator> evaluator_;
  std::vector<InstallableStatusCode> errors_;
};

TEST_F(InstallableEvaluatorUnitTest, EmptyManifestIsInvalid) {
  SetManifest(blink::mojom::Manifest::New());
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_EMPTY, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, CheckMinimalValidManifest) {
  SetManifest(GetValidManifest());
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresNameOrShortName) {
  SetManifest(GetValidManifest());

  manifest()->name = absl::nullopt;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->name = u"foo";
  manifest()->short_name = absl::nullopt;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->name = absl::nullopt;
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_NAME_OR_SHORT_NAME, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresNonEmptyNameORShortName) {
  SetManifest(GetValidManifest());

  manifest()->name = std::u16string();
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->name = u"foo";
  manifest()->short_name = std::u16string();
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->name = std::u16string();
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_NAME_OR_SHORT_NAME, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresValidStartURL) {
  SetManifest(GetValidManifest());

  manifest()->start_url = GURL();
  manifest()->id = GURL();
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(START_URL_NOT_VALID, GetErrorCode());

  manifest()->start_url = GURL("/");
  manifest()->id = GURL("/");
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(START_URL_NOT_VALID, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImagePNG) {
  SetManifest(GetValidManifest());

  manifest()->icons[0].type = u"image/gif";
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  manifest()->icons[0].type.clear();
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If the type is null, the icon src will be checked instead.
  manifest()->icons[0].src = GURL("http://example.com/icon.png");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Capital file extension is also permissible.
  manifest()->icons[0].src = GURL("http://example.com/icon.PNG");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Unsupported extensions are rejected.
  manifest()->icons[0].src = GURL("http://example.com/icon.gif");
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImageSVG) {
  SetManifest(GetValidManifest());

  // The correct mimetype is image/svg+xml.
  manifest()->icons[0].type = u"image/svg";
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If the type is null, the icon src will be checked instead.
  manifest()->icons[0].type.clear();
  manifest()->icons[0].src = GURL("http://example.com/icon.svg");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Capital file extension is also permissible.
  manifest()->icons[0].src = GURL("http://example.com/icon.SVG");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImageWebP) {
  SetManifest(GetValidManifest());

  manifest()->icons[0].type = u"image/webp";
  manifest()->icons[0].src = GURL("http://example.com/");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // If the type is null, the icon src is checked instead.
  // Case is ignored.
  manifest()->icons[0].type.clear();
  manifest()->icons[0].src = GURL("http://example.com/icon.wEBp");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresPurposeAny) {
  SetManifest(GetValidManifest());

  // The icon MUST have IconPurpose::ANY at least.
  manifest()->icons[0].purpose[0] = IconPurpose::MASKABLE;
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If one of the icon purposes match the requirement, it should be accepted.
  manifest()->icons[0].purpose.push_back(IconPurpose::ANY);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresIconSize) {
  SetManifest(GetValidManifest());

  // The icon MUST be 144x144 size at least.
  manifest()->icons[0].sizes[0] = gfx::Size(1, 1);
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  manifest()->icons[0].sizes[0] = gfx::Size(143, 143);
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  // If one of the sizes match the requirement, it should be accepted.
  manifest()->icons[0].sizes.emplace_back(144, 144);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Higher than the required size is okay.
  manifest()->icons[0].sizes[1] = gfx::Size(200, 200);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Icon size matching the maximum size requirement is correct.
  manifest()->icons[0].sizes[1] = gfx::Size(1024, 1024);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // Icon size larger than maximum size 1024x1024 should not
  // be accepted on desktop.
  manifest()->icons[0].sizes[1] = gfx::Size(1025, 1025);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
#else
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());
#endif  // BUILDFLAG(IS_ANDROID)

  // Non-square is okay.
  manifest()->icons[0].sizes[1] = gfx::Size(144, 200);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  // The representation of the keyword 'any' should be recognized.
  manifest()->icons[0].sizes[1] = gfx::Size(0, 0);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestDisplayModes) {
  SetManifest(GetValidManifest());

  manifest()->display = blink::mojom::DisplayMode::kUndefined;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED, GetErrorCode());

  manifest()->display = blink::mojom::DisplayMode::kBrowser;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED, GetErrorCode());

  manifest()->display = blink::mojom::DisplayMode::kMinimalUi;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display = blink::mojom::DisplayMode::kStandalone;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display = blink::mojom::DisplayMode::kFullscreen;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display = blink::mojom::DisplayMode::kWindowControlsOverlay;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display = blink::mojom::DisplayMode::kTabbed;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ManifestDisplayOverride) {
  SetManifest(GetValidManifest());

  manifest()->display_override.push_back(blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display_override.push_back(blink::mojom::DisplayMode::kBrowser);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kStandalone);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kStandalone);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kBrowser);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED, GetErrorCode());

  manifest()->display_override.insert(
      manifest()->display_override.begin(),
      blink::mojom::DisplayMode::kWindowControlsOverlay);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kTabbed);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, FallbackToBrowser) {
  SetManifest(GetValidManifest());

  manifest()->display = blink::mojom::DisplayMode::kBrowser;
  manifest()->display_override.push_back(blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, SupportWindowControlsOverlay) {
  SetManifest(GetValidManifest());

  manifest()->display_override.push_back(
      blink::mojom::DisplayMode::kWindowControlsOverlay);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

class InstallableEvaluatorUnitTest_Tabbed
    : public InstallableEvaluatorUnitTest {
 public:
  InstallableEvaluatorUnitTest_Tabbed() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kDesktopPWAsTabStrip};
};

TEST_F(InstallableEvaluatorUnitTest_Tabbed, SupportTabbed) {
  SetManifest(GetValidManifest());

  manifest()->display_override.push_back(blink::mojom::DisplayMode::kTabbed);
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ValidManifestValidMetadata) {
  SetManifest(GetValidManifest());
  SetMetadata(GetWebPageMetadata());

  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ValidManifestEmptyMetadata) {
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, EmptyManifestValidMetadata) {
  SetManifest(blink::mojom::Manifest::New());
  SetMetadata(GetWebPageMetadata());

  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(MANIFEST_EMPTY, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ValidMetadata) {
  // Non-empty manifest with only the "display" field, with valid metadata
  // is installable.
  SetManifest(blink::mojom::Manifest::New());
  manifest()->display = blink::mojom::DisplayMode::kStandalone;
  SetMetadata(GetWebPageMetadata());
  AddFavicon();

  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ImplicitAppName) {
  // Test that a site is installable when no manifest name but an "name" meta
  // tag is provided.
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  manifest()->name = std::u16string();
  manifest()->short_name = absl::nullopt;
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(MANIFEST_MISSING_NAME_OR_SHORT_NAME, GetErrorCode());

  SetMetadata(mojom::WebPageMetadata::New());
  metadata()->application_name = u"Name";
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());

  SetMetadata(mojom::WebPageMetadata::New());
  metadata()->title = u"Title";
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, MetadataStartUrl) {
  // Test that a site is installable when no manifest start_url but an
  // "application-url" meta tag is provided.
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  manifest()->start_url = GURL();
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(START_URL_NOT_VALID, GetErrorCode());

  metadata()->application_url = GURL("http://example.com");
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ImplicitIcons) {
  // Test that a site is installable when no manifest start_url but has valid
  // favicon.
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  manifest()->icons.clear();
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON, GetErrorCode());

  AddFavicon();
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

TEST_F(InstallableEvaluatorUnitTest, ImplicitDisplayMode) {
  // Test that a site is installable when manifest display mode is not
  // explicitly set to "browser".
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  // DisplayMode::kBrowser is not installable.
  manifest()->display = blink::mojom::DisplayMode::kBrowser;
  EXPECT_FALSE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED, GetErrorCode());

  // Everything else can be installable.
  manifest()->display = blink::mojom::DisplayMode::kUndefined;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
  manifest()->display = blink::mojom::DisplayMode::kMinimalUi;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
  manifest()->display = blink::mojom::DisplayMode::kStandalone;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
  manifest()->display = blink::mojom::DisplayMode::kFullscreen;
  EXPECT_TRUE(
      IsWebAppInstallable(InstallableCriteria::kImplicitManifestFieldsHTML));
  EXPECT_EQ(NO_ERROR_DETECTED, GetErrorCode());
}

}  // namespace webapps
