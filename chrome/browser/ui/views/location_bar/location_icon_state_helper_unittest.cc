// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#endif

using location_bar::ExecutePasteAndGo;
using location_bar::GetSecurityChipAccessibilityState;
using location_bar::GetSecurityChipText;
using location_bar::GetSecurityChipTooltipText;
using location_bar::ShouldAnimateSecurityChipTextChange;
using location_bar::ShouldShowSecurityChipText;
using ::testing::_;

class SecurityChipStateHelperTest : public testing::Test {
 public:
  TestLocationBarModel* model() { return &model_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestLocationBarModel model_;
};

TEST_F(SecurityChipStateHelperTest, HidesTextWhenEditing) {
  model()->set_url(GURL("https://example.com"));
  model()->set_secure_display_text(u"Secure");

  EXPECT_TRUE(
      GetSecurityChipText(model(), nullptr, /*is_editing_or_empty=*/true)
          .empty());
  EXPECT_FALSE(ShouldShowSecurityChipText(model(), /*web_contents=*/nullptr,
                                          /*is_editing_or_empty=*/true));
}

TEST_F(SecurityChipStateHelperTest, ShowsFileScheme) {
  model()->set_url(GURL("file:///path/to/file"));

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE),
      GetSecurityChipText(model(), nullptr, /*is_editing_or_empty=*/false));
  EXPECT_TRUE(ShouldShowSecurityChipText(model(), /*web_contents=*/nullptr,
                                         /*is_editing_or_empty=*/false));
}

TEST_F(SecurityChipStateHelperTest, ShowsChromeUIScheme) {
  model()->set_url(GURL("chrome://settings"));

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME),
      GetSecurityChipText(model(), nullptr, /*is_editing_or_empty=*/false));
  EXPECT_TRUE(ShouldShowSecurityChipText(model(), /*web_contents=*/nullptr,
                                         /*is_editing_or_empty=*/false));
}

TEST_F(SecurityChipStateHelperTest, ShowsSecureTextFallback) {
  model()->set_url(GURL("https://example.com"));
  model()->set_secure_display_text(u"Not Secure");

  EXPECT_EQ(u"Not Secure", GetSecurityChipText(model(), nullptr,
                                               /*is_editing_or_empty=*/false));
  EXPECT_TRUE(ShouldShowSecurityChipText(model(), /*web_contents=*/nullptr,
                                         /*is_editing_or_empty=*/false));
}

TEST_F(SecurityChipStateHelperTest, ShouldAnimateTextChange) {
  // When editing, animations should be suppressed.
  EXPECT_FALSE(ShouldAnimateSecurityChipTextChange(
      /*is_editing_or_empty=*/true, security_state::SecurityLevel::SECURE,
      security_state::SecurityLevel::WARNING));

  // Normal transitions when not editing.
  EXPECT_TRUE(ShouldAnimateSecurityChipTextChange(
      /*is_editing_or_empty=*/false, security_state::SecurityLevel::SECURE,
      security_state::SecurityLevel::WARNING));

  EXPECT_TRUE(ShouldAnimateSecurityChipTextChange(
      /*is_editing_or_empty=*/false, security_state::SecurityLevel::SECURE,
      security_state::SecurityLevel::DANGEROUS));

  // Suppress messy transitions.
  EXPECT_FALSE(ShouldAnimateSecurityChipTextChange(
      /*is_editing_or_empty=*/false, security_state::SecurityLevel::WARNING,
      security_state::SecurityLevel::DANGEROUS));
}

TEST_F(SecurityChipStateHelperTest, AccessibilityState) {
  // When editing or empty.
  auto editing_state = GetSecurityChipAccessibilityState(
      model(), /*is_editing_or_empty=*/true, u"Label");
  EXPECT_EQ(ax::mojom::Role::kImage, editing_state.role);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ACC_SEARCH_ICON), editing_state.name);
  EXPECT_TRUE(editing_state.description.empty());

  // When passive and label is empty.
  // TestLocationBarModel returns empty for GetSecureAccessibilityText().
  auto passive_empty_label_state = GetSecurityChipAccessibilityState(
      model(), /*is_editing_or_empty=*/false, u"");
  EXPECT_EQ(ax::mojom::Role::kPopUpButton, passive_empty_label_state.role);
  EXPECT_TRUE(passive_empty_label_state.name.empty());
  EXPECT_TRUE(passive_empty_label_state.description.empty());

  // When passive and label is not empty.
  auto passive_with_label_state = GetSecurityChipAccessibilityState(
      model(), /*is_editing_or_empty=*/false, u"Label");
  EXPECT_EQ(ax::mojom::Role::kPopUpButton, passive_with_label_state.role);
  EXPECT_EQ(u"Label", passive_with_label_state.name);
  EXPECT_TRUE(passive_with_label_state.description.empty());
}

TEST_F(SecurityChipStateHelperTest, TooltipText) {
  EXPECT_TRUE(GetSecurityChipTooltipText(
                  /*is_editing_or_empty=*/true)
                  .empty());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON),
            GetSecurityChipTooltipText(
                /*is_editing_or_empty=*/false));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
TEST_F(SecurityChipStateHelperTest, MaybeGetGradientGoogleSuperGIcon) {
  ui::ImageModel empty_icon = ui::ImageModel();
  EXPECT_FALSE(location_bar::MaybeGetGradientGoogleSuperGIcon(empty_icon));

  ui::ImageModel vector_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? omnibox::kInfoIcon
                                        : omnibox::kHttpOldIcon);
  EXPECT_FALSE(location_bar::MaybeGetGradientGoogleSuperGIcon(vector_icon));

  gfx::ImageSkia target_16 =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GOOGLE_G_GRADIENT_16_ALT);
  ui::ImageModel gradient_icon = ui::ImageModel::FromImageSkia(target_16);
  EXPECT_EQ(IDR_GOOGLE_G_GRADIENT_16_ALT,
            location_bar::MaybeGetGradientGoogleSuperGIcon(gradient_icon));

  gfx::ImageSkia target_20 =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GOOGLE_G_GRADIENT_20);
  ui::ImageModel gradient_icon_20 = ui::ImageModel::FromImageSkia(target_20);
  EXPECT_EQ(IDR_GOOGLE_G_GRADIENT_20,
            location_bar::MaybeGetGradientGoogleSuperGIcon(gradient_icon_20));
}
#endif

struct ExecutePasteAndGoTestCase {
  std::string_view test_name;
  std::u16string_view pasted_text;
  std::optional<std::string_view> expected_destination_url;
};

class ExecutePasteAndGoSchemeTest
    : public SecurityChipStateHelperTest,
      public testing::WithParamInterface<ExecutePasteAndGoTestCase> {};

TEST_P(ExecutePasteAndGoSchemeTest, FiltersClipboardSchemes) {
  OmniboxController omnibox_controller(std::make_unique<TestOmniboxClient>());
  TestOmniboxClient* const client =
      static_cast<TestOmniboxClient*>(omnibox_controller.client());
  const ExecutePasteAndGoTestCase& test_case = GetParam();

  if (test_case.expected_destination_url.has_value()) {
    EXPECT_CALL(*client,
                OnAutocompleteAccept(GURL(*test_case.expected_destination_url),
                                     _, _, _, _, _, _, _, _, _, _));
  } else {
    EXPECT_CALL(*client, OnAutocompleteAccept(_, _, _, _, _, _, _, _, _, _, _))
        .Times(0);
  }

  ExecutePasteAndGo(omnibox_controller, client->autocomplete_classifier(),
                    std::u16string(test_case.pasted_text),
                    base::TimeTicks::Now());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ExecutePasteAndGoSchemeTest,
    testing::Values(
        ExecutePasteAndGoTestCase{"Http", u"http://example.com/",
                                  "http://example.com/"},
        ExecutePasteAndGoTestCase{"Https", u"https://example.com/",
                                  "https://example.com/"},
        ExecutePasteAndGoTestCase{"Ftp", u"ftp://example.com/", std::nullopt},
        ExecutePasteAndGoTestCase{"File", u"file:///etc/passwd", std::nullopt},
        ExecutePasteAndGoTestCase{"ChromeUI", u"chrome://version",
                                  std::nullopt},
        ExecutePasteAndGoTestCase{
            "Filesystem", u"filesystem:https://example.com/temporary/test.html",
            std::nullopt},
        ExecutePasteAndGoTestCase{
            "Blob",
            u"blob:https://example.com/550e8400-e29b-41d4-a716-446655440000",
            std::nullopt},
        ExecutePasteAndGoTestCase{
            "JavascriptFilesystem",
            u"javascript:filesystem:https://example.com/temporary/test.html",
            std::nullopt},
        ExecutePasteAndGoTestCase{"JavascriptEmpty", u"javascript:",
                                  std::nullopt},
        ExecutePasteAndGoTestCase{"AboutBlank", u"about:blank", std::nullopt},
        ExecutePasteAndGoTestCase{
            "ViewSource", u"view-source:https://example.com/", std::nullopt},
        ExecutePasteAndGoTestCase{"Ws", u"ws://example.com/socket",
                                  std::nullopt},
        ExecutePasteAndGoTestCase{"Wss", u"wss://example.com/socket",
                                  std::nullopt},
        ExecutePasteAndGoTestCase{"DevTools",
                                  u"devtools://devtools/bundled/inspector.html",
                                  std::nullopt},
        ExecutePasteAndGoTestCase{"Whitespace", u"   ", std::nullopt}),
    [](const testing::TestParamInfo<ExecutePasteAndGoTestCase>& info) {
      return std::string(info.param.test_name);
    });

TEST_F(SecurityChipStateHelperTest,
       ExecutePasteAndGoUsesClientPageClassification) {
  OmniboxController omnibox_controller(std::make_unique<TestOmniboxClient>());
  TestOmniboxClient* const client =
      static_cast<TestOmniboxClient*>(omnibox_controller.client());
  client->location_bar_model()->set_page_classification(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);

  ExecutePasteAndGo(omnibox_controller, client->autocomplete_classifier(),
                    u"filesystem:https://example.com/temporary/test.html",
                    base::TimeTicks::Now());

  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      client->autocomplete_classifier()
          ->autocomplete_controller()
          ->input()
          .current_page_classification());
}
