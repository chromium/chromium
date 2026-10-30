// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
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

  static blink::Manifest::Builder GetValidManifestBuilder() {
    blink::Manifest::ImageResource primary_icon;
    primary_icon.src = GURL("http://example.com/icon.png");
    primary_icon.type = u"image/png";
    primary_icon.sizes.emplace_back(144, 144);
    primary_icon.purpose.push_back(IconPurpose::ANY);

    return blink::Manifest::Builder(GURL("http://example.com"))
        .SetName(u"foo")
        .SetShortName(u"bar")
        .SetScope(GURL("http://example.com"))
        .SetHasValidSpecifiedStartUrl(true)
        .SetDisplay(blink::mojom::DisplayMode::kStandalone)
        .SetIcons({primary_icon});
  }

  static blink::Manifest GetValidManifest() {
    return GetValidManifestBuilder().BuildChecked();
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

  // Builds and sets the default manifest for the given document url.
  void SetManifestAsDefault(const GURL& document_url) {
    page_data_->manifest_->fetched = false;
    page_data_->OnManifestFetched(blink::Manifest::CreateDefault(document_url),
                                  /*manifest_url=*/GURL(),
                                  InstallableStatusCode::NO_ERROR_DETECTED);
  }

  void SetManifestParsingOrNetworkError() {
    page_data_->manifest_->fetched = false;
    page_data_->OnManifestFetched(
        std::nullopt,
        /*manifest_url=*/GURL(),
        InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
  }

  void SetManifest(blink::Manifest manifest) {
    page_data_->manifest_->fetched = false;
    GURL manifest_url("http://example.com");
    page_data_->OnManifestFetched(std::move(manifest), manifest_url,
                                  InstallableStatusCode::NO_ERROR_DETECTED);
  }

  void SetManifest(blink::Manifest::Builder builder) {
    SetManifest(std::move(builder).BuildChecked());
  }

  blink::Manifest::Builder ManifestBuilder() {
    CHECK(page_data_->GetManifest());
    return page_data_->GetManifest()->ToBuilder();
  }

  void SetIcon(blink::Manifest::ImageResource icon) {
    SetManifest(ManifestBuilder().SetIcons({std::move(icon)}));
  }

  void SetMetadata(mojom::WebPageMetadataPtr metadata) {
    page_data_->web_page_metadata_->fetched = false;
    page_data_->OnPageMetadataFetched(std::move(metadata));
  }

  base::optional_ref<const blink::Manifest> manifest() {
    return page_data_->GetManifest();
  }

  mojom::WebPageMetadata* metadata() {
    return page_data_->web_page_metadata_->metadata.get();
  }

  InstallableStatusCode GetCheckInstallabilityErrorCode(
      InstallableCriteria criteria) {
    InstallableEvaluator evaluator(web_contents(), *page_data_, criteria);
    auto errors = evaluator.CheckInstallability();
    return errors.empty() ? InstallableStatusCode::NO_ERROR_DETECTED
                          : errors[0];
  }

 private:
  std::unique_ptr<InstallablePageData> page_data_;
};

TEST_F(InstallableEvaluatorUnitTest, DoNotCheck) {
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
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

TEST_P(InstallableEvaluatorCriteriaUnitTest, UnsetManifest) {
  web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  SetManifestAsDefault(GURL("https://www.example.com"));
  TestCheckInstallability(
      InstallableStatusCode::NO_MANIFEST, InstallableStatusCode::NO_MANIFEST,
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/path/page.html"));
  SetManifestAsDefault(GURL("https://www.example.com/path/page.html"));
  TestCheckInstallability(InstallableStatusCode::NO_MANIFEST,
                          InstallableStatusCode::NO_MANIFEST,
                          InstallableStatusCode::NO_MANIFEST);
}

TEST_P(InstallableEvaluatorCriteriaUnitTest, ManifestParsingOrNetworkError) {
  SetManifestParsingOrNetworkError();
  TestCheckInstallability(
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR,
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR,
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/path/page.html"));
  TestCheckInstallability(
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR,
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR,
      InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR);
}

TEST_P(InstallableEvaluatorCriteriaUnitTest, CheckStartUrl) {
  web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());
  // Valid manifest start_url
  SetManifest(ManifestBuilder()
                  .SetStartUrl(GURL("https://www.example.com"))
                  .SetId(GURL("https://www.example.com"))
                  .SetScope(GURL("https://www.example.com"))
                  .SetHasValidSpecifiedStartUrl(true));
  TestCheckInstallability(InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED);

  // No valid specified start_url, but has default manifest start_url.
  SetManifest(ManifestBuilder().SetHasValidSpecifiedStartUrl(false));
  InstallableStatusCode expected_url_result =
      InstallableStatusCode::NO_ERROR_DETECTED;
  TestCheckInstallability(InstallableStatusCode::START_URL_NOT_VALID,
                          expected_url_result,
                          InstallableStatusCode::NO_ERROR_DETECTED);

  // Valid application_url
  metadata()->application_url = GURL("http://example.com");
  SetManifest(ManifestBuilder().SetHasValidSpecifiedStartUrl(false));
  TestCheckInstallability(InstallableStatusCode::START_URL_NOT_VALID,
                          InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED);

  // No start_url, root scope page
  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/pageA"));
  metadata()->application_url = GURL();
  SetManifest(ManifestBuilder().SetHasValidSpecifiedStartUrl(false));
  TestCheckInstallability(InstallableStatusCode::START_URL_NOT_VALID,
                          expected_url_result,
                          InstallableStatusCode::NO_ERROR_DETECTED);

  // No valid specified start_url, but has default manifest start_url, Not root
  // scope page
  web_contents_tester()->NavigateAndCommit(
      GURL("https://www.example.com/path/pageB"));
  SetManifest(ManifestBuilder()
                  .SetStartUrl(GURL("https://www.example.com/pageB"))
                  .SetId(GURL("https://www.example.com/pageB"))
                  .SetScope(GURL("https://www.example.com/"))
                  .SetHasValidSpecifiedStartUrl(false));
  TestCheckInstallability(InstallableStatusCode::START_URL_NOT_VALID,
                          expected_url_result, expected_url_result);
}

TEST_P(InstallableEvaluatorCriteriaUnitTest, CheckNameOrShortName) {
  SetManifest(GetValidManifest());

  SetManifest(ManifestBuilder().SetName(std::nullopt).SetShortName(u"bar"));
  TestCheckInstallability(InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED);

  SetManifest(ManifestBuilder().SetName(u"foo").SetShortName(std::nullopt));
  TestCheckInstallability(InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED,
                          InstallableStatusCode::NO_ERROR_DETECTED);

  SetManifest(
      ManifestBuilder().SetName(std::nullopt).SetShortName(std::nullopt));
  TestCheckInstallability(
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  SetMetadata(mojom::WebPageMetadata::New());
  SetManifest(ManifestBuilder().SetName(u"").SetShortName(u""));
  metadata()->application_name = std::u16string();
  metadata()->title = std::u16string();
  TestCheckInstallability(
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  metadata()->application_name = u"Name";
  TestCheckInstallability(
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
      InstallableStatusCode::NO_ERROR_DETECTED,
      InstallableStatusCode::NO_ERROR_DETECTED);

  metadata()->application_name = std::u16string();
  metadata()->title = u"Title";
  TestCheckInstallability(
      InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
      InstallableStatusCode::NO_ERROR_DETECTED,
      InstallableStatusCode::NO_ERROR_DETECTED);
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImagePNG) {
  SetManifest(GetValidManifest());

  blink::Manifest::ImageResource icon;
  icon.src = GURL("http://example.com/icon");
  icon.sizes.emplace_back(144, 144);
  icon.purpose.push_back(IconPurpose::ANY);

  icon.type = u"image/gif";
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  icon.type.clear();
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If the type is null, the icon src will be checked instead.
  icon.src = GURL("http://example.com/icon.png");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Capital file extension is also permissible.
  icon.src = GURL("http://example.com/icon.PNG");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Unsupported extensions are rejected.
  icon.src = GURL("http://example.com/icon.gif");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImageSVG) {
  SetManifest(GetValidManifest());

  blink::Manifest::ImageResource icon;
  icon.src = GURL("http://example.com/icon.svg");
  icon.sizes.emplace_back(144, 144);
  icon.purpose.push_back(IconPurpose::ANY);

  // The correct mimetype is image/svg+xml.
  icon.type = u"image/svg";
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If the type is null, the icon src will be checked instead.
  icon.type.clear();
  icon.src = GURL("http://example.com/icon.svg");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Capital file extension is also permissible.
  icon.src = GURL("http://example.com/icon.SVG");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestSupportsImageWebP) {
  SetManifest(GetValidManifest());

  blink::Manifest::ImageResource icon;
  icon.sizes.emplace_back(144, 144);
  icon.purpose.push_back(IconPurpose::ANY);

  icon.type = u"image/webp";
  icon.src = GURL("http://example.com/");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If the type is null, the icon src is checked instead.
  // Case is ignored.
  icon.type.clear();
  icon.src = GURL("http://example.com/icon.wEBp");
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresPurposeAny) {
  SetManifest(GetValidManifest());

  blink::Manifest::ImageResource icon;
  icon.src = GURL("http://example.com/icon.png");
  icon.type = u"image/png";
  icon.sizes.emplace_back(144, 144);

  // The icon MUST have IconPurpose::ANY at least.
  icon.purpose = {IconPurpose::MASKABLE};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If one of the icon purposes match the requirement, it should be accepted.
  icon.purpose = {IconPurpose::MASKABLE, IconPurpose::ANY};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestRequiresIconSize) {
  SetManifest(GetValidManifest());

  blink::Manifest::ImageResource icon;
  icon.src = GURL("http://example.com/icon.png");
  icon.type = u"image/png";
  icon.purpose.push_back(IconPurpose::ANY);

  // The icon MUST be 144x144 size at least.
  icon.sizes = {gfx::Size(1, 1)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  icon.sizes = {gfx::Size(143, 143)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // If one of the sizes match the requirement, it should be accepted.
  icon.sizes = {gfx::Size(143, 143), gfx::Size(144, 144)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Non-square icons are not considered installable, regardless of orientation.
  icon.sizes = {gfx::Size(143, 143), gfx::Size(144, 200)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  icon.sizes = {gfx::Size(143, 143), gfx::Size(192, 144)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Higher than the required size is okay.
  icon.sizes = {gfx::Size(143, 143), gfx::Size(200, 200)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Icon size matching the maximum size requirement is correct.
  icon.sizes = {gfx::Size(143, 143), gfx::Size(1024, 1024)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));

  // Icon size larger than maximum size 1024x1024 should not
  // be accepted on desktop.
  icon.sizes = {gfx::Size(143, 143), gfx::Size(1025, 1025)};
  SetIcon(icon);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
#else
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
#endif  // BUILDFLAG(IS_ANDROID)

  // The representation of the keyword 'any' should be recognized.
  icon.sizes = {gfx::Size(143, 143), gfx::Size(0, 0)};
  SetIcon(icon);
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestDisplayModes) {
  SetManifest(GetValidManifest());

  SetManifest(
      ManifestBuilder().SetDisplay(blink::mojom::DisplayMode::kUndefined));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  SetManifest(
      ManifestBuilder().SetDisplay(blink::mojom::DisplayMode::kBrowser));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  SetManifest(
      ManifestBuilder().SetDisplay(blink::mojom::DisplayMode::kMinimalUi));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  SetManifest(
      ManifestBuilder().SetDisplay(blink::mojom::DisplayMode::kStandalone));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  SetManifest(
      ManifestBuilder().SetDisplay(blink::mojom::DisplayMode::kFullscreen));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  SetManifest(ManifestBuilder().SetDisplay(
      blink::mojom::DisplayMode::kWindowControlsOverlay));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  SetManifest(ManifestBuilder().SetDisplay(blink::mojom::DisplayMode::kTabbed));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, ManifestDisplayOverride) {
  SetManifest(GetValidManifest());

  std::vector<blink::Manifest::DisplayOverride> overrides = {
      blink::Manifest::DisplayOverride::Create(
          blink::mojom::DisplayMode::kMinimalUi)};
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  overrides.push_back(blink::Manifest::DisplayOverride::Create(
      blink::mojom::DisplayMode::kBrowser));
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  overrides.insert(overrides.begin(),
                   blink::Manifest::DisplayOverride::Create(
                       blink::mojom::DisplayMode::kStandalone));
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  overrides.insert(overrides.begin(),
                   blink::Manifest::DisplayOverride::Create(
                       blink::mojom::DisplayMode::kStandalone));
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  overrides.insert(overrides.begin(), blink::Manifest::DisplayOverride::Create(
                                          blink::mojom::DisplayMode::kBrowser));
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  overrides.insert(overrides.begin(),
                   blink::Manifest::DisplayOverride::Create(
                       blink::mojom::DisplayMode::kWindowControlsOverlay));
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  overrides.insert(overrides.begin(), blink::Manifest::DisplayOverride::Create(
                                          blink::mojom::DisplayMode::kTabbed));
  SetManifest(ManifestBuilder().SetDisplayOverride(overrides));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestIgnoreDisplay));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, FallbackToBrowser) {
  SetManifest(GetValidManifest());

  SetManifest(ManifestBuilder()
                  .SetDisplay(blink::mojom::DisplayMode::kBrowser)
                  .SetDisplayOverride({blink::Manifest::DisplayOverride::Create(
                      blink::mojom::DisplayMode::kMinimalUi)}));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, SupportWindowControlsOverlay) {
  SetManifest(GetValidManifest());

  SetManifest(ManifestBuilder().SetDisplayOverride(
      {blink::Manifest::DisplayOverride::Create(
          blink::mojom::DisplayMode::kWindowControlsOverlay)}));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
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

  SetManifest(ManifestBuilder().SetDisplayOverride(
      {blink::Manifest::DisplayOverride::Create(
          blink::mojom::DisplayMode::kTabbed)}));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kValidManifestWithIcons));
}

TEST_F(InstallableEvaluatorUnitTest, ValidManifestValidMetadata) {
  SetManifest(GetValidManifest());
  SetMetadata(GetWebPageMetadata());

  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, ValidMetadata) {
  // Non-empty manifest with only the "display" field, with valid metadata
  // is installable.
  SetManifest(blink::Manifest::Builder(GURL("http://example.com"))
                  .SetDisplay(blink::mojom::DisplayMode::kStandalone));
  SetMetadata(GetWebPageMetadata());
  AddFavicon();

  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

TEST_F(InstallableEvaluatorUnitTest, ValidMetadataRootScopePage) {
  // Test that a root-scoped page, with no manifest and a valid metadata is
  // installable.
  web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  SetManifestAsDefault(GURL("https://www.example.com"));
  SetMetadata(GetWebPageMetadata());
  AddFavicon();

  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kNoManifestAtRootScope));
}

TEST_F(InstallableEvaluatorUnitTest, ImplicitIcons) {
  // Test that a site is installable when no manifest start_url but has valid
  // favicon.
  SetManifest(GetValidManifest());
  SetMetadata(mojom::WebPageMetadata::New());

  SetManifest(ManifestBuilder().SetIcons({}));
  EXPECT_EQ(InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));

  AddFavicon();
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            GetCheckInstallabilityErrorCode(
                InstallableCriteria::kImplicitManifestFieldsHTML));
}

}  // namespace webapps
