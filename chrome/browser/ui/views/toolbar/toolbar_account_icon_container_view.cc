// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

ToolbarAccountIconContainerView::ToolbarAccountIconContainerView(
    BrowserView* browser_view)
    : ToolbarIconContainerView(
          /*uses_highlight=*/!browser_view->browser()
              ->profile()
              ->IsIncognitoProfile()),
      avatar_(new AvatarToolbarButton(browser_view, this)),
      browser_(browser_view->browser()) {
  PageActionIconParams params;
  params.types_enabled = {
      PageActionIconType::kManagePasswords,
      PageActionIconType::kLocalCardMigration,
      PageActionIconType::kSaveCard,
  };
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAddressProfileSavePrompt)) {
    // TODO(crbug.com/1167060): Place this in the proper order upon having
    // final mocks.
    params.types_enabled.push_back(PageActionIconType::kSaveAutofillAddress);
  }
  params.browser = browser_;
  params.command_updater = browser_->command_controller();
  params.icon_label_bubble_delegate = this;
  params.page_action_icon_delegate = this;
  params.button_observer = this;
  AddMainButton(avatar_);

  // Since the insertion point for icons before the avatar button, we don't
  // initialize until after the avatar button has been added.
  page_action_icon_controller_ = std::make_unique<PageActionIconController>();
  page_action_icon_controller_->Init(params, this);

  avatar_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
}

ToolbarAccountIconContainerView::~ToolbarAccountIconContainerView() = default;

void ToolbarAccountIconContainerView::UpdateAllIcons() {
  page_action_icon_controller_->SetIconColor(GetIconColor());
  page_action_icon_controller_->UpdateAll();
  avatar_->UpdateIcon();
}

SkColor
ToolbarAccountIconContainerView::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return GetIconColor();
}

SkColor ToolbarAccountIconContainerView::GetIconLabelBubbleInkDropColor()
    const {
  return GetToolbarInkDropBaseColor(this);
}

SkColor ToolbarAccountIconContainerView::GetIconLabelBubbleBackgroundColor()
    const {
  return GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);
}

float ToolbarAccountIconContainerView::GetPageActionInkDropVisibleOpacity()
    const {
  return kToolbarInkDropVisibleOpacity;
}

content::WebContents*
ToolbarAccountIconContainerView::GetWebContentsForPageActionIconView() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

gfx::Insets ToolbarAccountIconContainerView::GetPageActionIconInsets(
    const PageActionIconView* icon_view) const {
  // Ideally, the icon should have the same ink drop shape as toolbar buttons.
  // TODO(crbug.com/1060250): fix actual inkdrop shape.
  return ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_LABEL_BUTTON);
}

void ToolbarAccountIconContainerView::OnThemeChanged() {
  ToolbarIconContainerView::OnThemeChanged();
  // Update icon color.
  UpdateAllIcons();
}

void ToolbarAccountIconContainerView::AddPageActionIcon(views::View* icon) {
  // Add the page action icons to the end of the container, just before the
  // avatar icon.
  AddChildViewAt(icon, GetIndexOf(avatar_));
}

BEGIN_METADATA(ToolbarAccountIconContainerView, ToolbarIconContainerView)
END_METADATA
