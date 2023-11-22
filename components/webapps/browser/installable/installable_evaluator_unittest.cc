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

  void SetManifest(blink::mojom::ManifestPtr manifest) {
    GURL manifest_url("http://example.com");
    page_data_->OnManifestFetched(std::move(manifest), manifest_url,
                                  NO_ERROR_DETECTED);
  }

  void SetMetadata(mojom::WebPageMetadataPtr metadata) {
    page_data_->web_page_metadata_->fetched = false;
    page_data_->OnPageMetadataFetched(std::move(metadata));
  }

  blink::mojom::Manifest* manifest() {
    return page_data_->manifest_->manifest.get();
  }

  mojom::WebPageMetadata* metadata() {
    return page_data_->web_page_metadata_->metadata.get();
  }

  absl::optional<InstallableStatusCode> GetCheckInstallabilityErrorCode(
      InstallableCriteria criteria) {
    InstallableEvaluator evaluator(web_contents(), *page_data_, criteria);
    auto errors = evaluator.CheckInstallability();
    if (!errors.has_value()) {
      return absl::nullopt;
    }
    return errors->empty() ? NO_ERROR_DETECTED : errors.value()[0];
  }

 private:
  std::unique_ptr<InstallablePageData> page_data_;
};

TEST_F(InstallableEvaluatorUnitTest, DoNotCheck) {
  EXPECT_EQ(absl::nullopt,
            GetCheckInstallabilityErrorCode(InstallableCriteria::kDoNotCheck));
}

class InstallableEvaluatorCriteriaUnitTest
    : public InstallableEvaluatorUnitTest,
      public testing::WithParamInterface<InstallableCriteria> {
 public:
  void TestCheckInstallability(InstallableStatusCode valid_manifest_code,
                               InstallableStatusCode implicit_fields_code,
                               InstallableStatusCode root_page_code) {
    auto error_code = GetCheckInstallabilityErrorCode(GetParam());
    switch (GetParam()) {
      case InstallableCriteria::kValidManifestWithIcons:
        EXPECT_EQ(valid_manifest_code, error_code);
        break;
      case InstallableCriteria::kImplicitManifestFieldsHTML:
        EXPECT_EQ(implicit_fields_code, error_code);
        break;
      case InstallableCriteria::kNoManifestAtRootScope:
        EXPECT_EQ(root_page_code, error_code);
        break;
      default:
        NOTREACHED();
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InstallableEvaluatorCriteriaUnitTest,
    testing::Values(InstallableCriteria::kValidManifestWithIcons,
                    InstallableCriteria::kImplicitManifestFieldsHTML,
                    InstallableCriteria::kNoManifestAtRootScope));

TEST_P(InstallableEvaluatorCriteriaUnitTest, NoManifest) {
  web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  TestCheckInstallability(NO_MANIFEST, NO_MANIFEST,
                          MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/path/page.html"));
  TestCheckInstallability(NO_MANIFEST, NO_MANIFEST, NO_MANIFEST);
}

TEST_P(InstallableEvaluatorCriteriaUnitTest, EmptyManifest) {
  SetManifest(blink::mojom::Manifest::New());
  TestCheckInstallability(MANIFEST_EMPTY, MANIFEST_EMPTY,
                          MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/path/page.html"));
  TestCheckInstallability(MANIFEST_EMPTY, MANIFEST_EMPTY, MANIFEST_EMPTY);
}

TEST_P(InstallableEvaluatorCriteriaUnitTest, CheckStartUrl) {
  web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());
  // Valid manifest start_url
  manifest()->start_url = GURL("https://www.example.com");
  TestCheckInstallability(NO_ERROR_DETECTED, NO_ERROR_DETECTED,
                          NO_ERROR_DETECTED);

  // No manifest start_url
  manifest()->start_url = GURL();
  TestCheckInstallability(START_URL_NOT_VALID, START_URL_NOT_VALID,
                          NO_ERROR_DETECTED);

  // manifest start_url invalid
  manifest()->start_url = GURL("/");
  TestCheckInstallability(START_URL_NOT_VALID, START_URL_NOT_VALID,
                          NO_ERROR_DETECTED);

  // Valid application_url
  metadata()->application_url = GURL("http://example.com");
  TestCheckInstallability(START_URL_NOT_VALID, NO_ERROR_DETECTED,
                          NO_ERROR_DETECTED);

  // No start_url, root scope page
  metadata()->application_url = GURL();
  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/pageA"));
  TestCheckInstallability(START_URL_NOT_VALID, START_URL_NOT_VALID,
                          NO_ERROR_DETECTED);

  // No start_url, Not root scope page
  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/path/pageB"));
  TestCheckInstallability(START_URL_NOT_VALID, START_URL_NOT_VALID,
                          START_URL_NOT_VALID);
}

TEST_P(InstallableEvaluatorCriteriaUnitTest, CheckNameOrShortName) {
  SetManifest(GetValidManifest());

  manifest()->name = absl::nullopt;
  manifest()->short_name = u"bar";
  TestCheckInstallability(NO_ERROR_DETECTED, NO_ERROR_DETECTED,
                          NO_ERROR_DETECTED);

  manifest()->name = u"foo";
  manifest()->short_name = absl::nullopt;
  TestCheckInstallability(NO_ERROR_DETECTED, NO_ERROR_DETECTED,
                          NO_ERROR_DETECTED);

  manifest()->name = absl::nullopt;
  manifest()->short_name = absl::nullopt;
  TestCheckInstallability(MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                          MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                          MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  SetMetadata(mojom::WebPageMetadata::New());
  manifest()->name = std::u16string();
  manifest()->short_name = std::u16string();
  metadata()->application_name = std::u16string();
  metadata()->title = std::u16string();
  TestCheckInstallability(MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                          MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                          MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  metadata()->application_name = u"Name";
  TestCheckInstallability(MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                          NO_ERROR_DETECTED, NO_ERROR_DETECTED);

  metadata()->application_name = std::u16string();
  metadata()->title = u"Title";
  TestCheckInstallability(MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                          NO_ERROR_DETECTED, NO_ERROR_DETECTED);
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImagePNG) {
  SetManifest(GetValidManifest());

  manifest()->icons[0].type = u"image/gif";
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  manifest()->icons[0].type.clear();
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If the type is null, the icon src will be checked instead.
  manifest()->icons[0].src = GURL("http://example.com/icon.png");
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Capital file extension is also permissible.
  manifest()->icons[0].src = GURL("http://example.com/icon.PNG");
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Unsupported extensions are rejected.
  manifest()->icons[0].src = GURL("http://example.com/icon.gif");
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImageSVG) {
  SetManifest(GetValidManifest());

  // The correct mimetype is image/svg+xml.
  manifest()->icons[0].type = u"image/svg";
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If the type is null, the icon src will be checked instead.
  manifest()->icons[0].type.clear();
  manifest()->icons[0].src = GURL("http://example.com/icon.svg");
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Capital file extension is also permissible.
  manifest()->icons[0].src = GURL("http://example.com/icon.SVG");
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImageWebP) {
  SetManifest(GetValidManifest());

  manifest()->icons[0].type = u"image/webp";
  manifest()->icons[0].src = GURL("http://example.com/");
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If the type is null, the icon src is checked instead.
  // Case is ignored.
  manifest()->icons[0].type.clear();
  manifest()->icons[0].src = GURL("http://example.com/icon.wEBp");
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresPurposeAny) {
  SetManifest(GetValidManifest());

  // The icon MUST have IconPurpose::ANY at least.
  manifest()->icons[0].purpose[0] = IconPurpose::MASKABLE;
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If one of the icon purposes match the requirement, it should be accepted.
  manifest()->icons[0].purpose.push_back(IconPurpose::ANY);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresIconSize) {
  SetManifest(GetValidManifest());

  // The icon MUST be 144x144 size at least.
  manifest()->icons[0].sizes[0] = gfx::Size(1, 1);
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  manifest()->icons[0].sizes[0] = gfx::Size(143, 143);
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If one of the sizes match the requirement, it should be accepted.
  manifest()->icons[0].sizes.emplace_back(144, 144);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Higher than the required size is okay.
  manifest()->icons[0].sizes[1] = gfx::Size(200, 200);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Icon size matching the maximum size requirement is correct.
  manifest()->icons[0].sizes[1] = gfx::Size(1024, 1024);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Icon size larger than maximum size 1024x1024 should not
  // be accepted on desktop.
  manifest()->icons[0].sizes[1] = gfx::Size(1025, 1025);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
#else
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
#endif  // BUILDFLAG(IS_ANDROID)

  // Non-square is okay.
  manifest()->icons[0].sizes[1] = gfx::Size(144, 200);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // The representation of the keyword 'any' should be recognized.
  manifest()->icons[0].sizes[1] = gfx::Size(0, 0);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestDisplayModes) {
  SetManifest(GetValidManifest());

  manifest()->display = blink::mojom::DisplayMode::kUndefined;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display = blink::mojom::DisplayMode::kBrowser;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_DISPLAY_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display = blink::mojom::DisplayMode::kMinimalUi;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display = blink::mojom::DisplayMode::kStandalone;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display = blink::mojom::DisplayMode::kFullscreen;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display = blink::mojom::DisplayMode::kWindowControlsOverlay;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display = blink::mojom::DisplayMode::kTabbed;
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestDisplayOverride) {
  SetManifest(GetValidManifest());

  manifest()->display_override.push_back(blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display_override.push_back(blink::mojom::DisplayMode::kBrowser);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kBrowser);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display_override.insert(
      manifest()->display_override.begin(),
      blink::mojom::DisplayMode::kWindowControlsOverlay);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  manifest()->display_override.insert(manifest()->display_override.begin(),
                                      blink::mojom::DisplayMode::kTabbed);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, FallbackToBrowser) {
  SetManifest(GetValidManifest());

  manifest()->display = blink::mojom::DisplayMode::kBrowser;
  manifest()->display_override.push_back(blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, SupportWindowControlsOverlay) {
  SetManifest(GetValidManifest());

  manifest()->display_override.push_back(
      blink::mojom::DisplayMode::kWindowControlsOverlay);
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
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
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ValidManifestValidMetadata) {
  SetManifest(GetValidManifest());
  SetMetadata(GetWebPageMetadata());

  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, ValidMetadata) {
  // Non-empty manifest with only the "display" field, with valid metadata
  // is installable.
  SetManifest(blink::mojom::Manifest::New());
  manifest()->display = blink::mojom::DisplayMode::kStandalone;
  SetMetadata(GetWebPageMetadata());
  AddFavicon();

  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, ValidMetadataRootScopePage) {
  // Test that a root-scoped page, with no manifest and a valid metadata is
  // installable.
  web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  SetMetadata(GetWebPageMetadata());
  AddFavicon();

  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kNoManifestAtRootScope));
}

TEST_F(InstallableEvaluatorUnitTest, ImplicitIcons) {
  // Test that a site is installable when no manifest start_url but has valid
  // favicon.
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  manifest()->icons.clear();
  EXPECT_EQ(MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  AddFavicon();
  EXPECT_EQ(NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

}  // namespace webapps
