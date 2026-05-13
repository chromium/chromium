// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"

#include "build/branding_buildflags.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#endif

using location_bar::SecurityChipIcon;

using location_bar::GetSecurityChipAccessibilityState;
using location_bar::GetSecurityChipIconEnum;
using location_bar::GetSecurityChipText;
using location_bar::GetSecurityChipTooltipText;
using location_bar::IsSecurityChipInteractive;
using location_bar::ShouldAnimateSecurityChipTextChange;
using location_bar::ShouldShowSecurityChipText;

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
  EXPECT_FALSE(
      ShouldShowSecurityChipText(model(), /*is_editing_or_empty=*/true));
}

TEST_F(SecurityChipStateHelperTest, ShowsFileScheme) {
  model()->set_url(GURL("file:///path/to/file"));

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE),
      GetSecurityChipText(model(), nullptr, /*is_editing_or_empty=*/false));
  EXPECT_TRUE(
      ShouldShowSecurityChipText(model(), /*is_editing_or_empty=*/false));
}

TEST_F(SecurityChipStateHelperTest, ShowsChromeUIScheme) {
  model()->set_url(GURL("chrome://settings"));

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME),
      GetSecurityChipText(model(), nullptr, /*is_editing_or_empty=*/false));
  EXPECT_TRUE(
      ShouldShowSecurityChipText(model(), /*is_editing_or_empty=*/false));
}

TEST_F(SecurityChipStateHelperTest, ShowsSecureTextFallback) {
  model()->set_url(GURL("https://example.com"));
  model()->set_secure_display_text(u"Not Secure");

  EXPECT_EQ(u"Not Secure", GetSecurityChipText(model(), nullptr,
                                               /*is_editing_or_empty=*/false));
  EXPECT_TRUE(
      ShouldShowSecurityChipText(model(), /*is_editing_or_empty=*/false));
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

TEST_F(SecurityChipStateHelperTest, SecurityChipIconEnum) {
  EXPECT_EQ(
      SecurityChipIcon::kAddContext,
      GetSecurityChipIconEnum(model(), /*is_add_context_button_shown=*/true));

  model()->set_security_level(security_state::SecurityLevel::SECURE);
  model()->set_icon(omnibox::kHttpIcon);
  EXPECT_EQ(
      SecurityChipIcon::kSecurePageInfo,
      GetSecurityChipIconEnum(model(), /*is_add_context_button_shown=*/false));

  model()->set_security_level(security_state::SecurityLevel::DANGEROUS);
  model()->set_icon(omnibox::kHttpIcon);
  EXPECT_EQ(
      SecurityChipIcon::kDangerous,
      GetSecurityChipIconEnum(model(), /*is_add_context_button_shown=*/false));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
  model()->set_icon(vector_icons::kGoogleSuperGIcon);
  EXPECT_EQ(
      SecurityChipIcon::kGoogleSuperG,
      GetSecurityChipIconEnum(model(), /*is_add_context_button_shown=*/false));
#endif
}

TEST_F(SecurityChipStateHelperTest, SecurityChipInteractivity) {
  EXPECT_FALSE(IsSecurityChipInteractive(
      /*is_editing_or_empty=*/true, SecurityChipIcon::kHttp));
  EXPECT_TRUE(IsSecurityChipInteractive(
      /*is_editing_or_empty=*/false, SecurityChipIcon::kHttp));
  EXPECT_FALSE(IsSecurityChipInteractive(
      /*is_editing_or_empty=*/false, SecurityChipIcon::kGoogleSuperG));
  EXPECT_FALSE(IsSecurityChipInteractive(
      /*is_editing_or_empty=*/false, SecurityChipIcon::kGoogleGMonochrome));
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
TEST_F(SecurityChipStateHelperTest, IsGradientGoogleSuperGIcon) {
  ui::ImageModel empty_icon = ui::ImageModel();
  EXPECT_FALSE(location_bar::IsGradientGoogleSuperGIcon(empty_icon));

  ui::ImageModel vector_icon =
      ui::ImageModel::FromVectorIcon(omnibox::kHttpIcon);
  EXPECT_FALSE(location_bar::IsGradientGoogleSuperGIcon(vector_icon));

  gfx::ImageSkia target_16 =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GOOGLE_G_GRADIENT_16_ALT);
  ui::ImageModel gradient_icon = ui::ImageModel::FromImageSkia(target_16);
  EXPECT_TRUE(location_bar::IsGradientGoogleSuperGIcon(gradient_icon));

  gfx::ImageSkia target_20 =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GOOGLE_G_GRADIENT_20);
  ui::ImageModel gradient_icon_20 = ui::ImageModel::FromImageSkia(target_20);
  EXPECT_TRUE(location_bar::IsGradientGoogleSuperGIcon(gradient_icon_20));
}
#endif
