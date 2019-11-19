// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// static
const char ToolbarAccountIconContainerView::
    kToolbarAccountIconContainerViewClassName[] =
        "ToolbarAccountIconContainerView";

ToolbarAccountIconContainerView::ToolbarAccountIconContainerView(
    Browser* browser)
    : ToolbarIconContainerView(
          /*uses_highlight=*/!browser->profile()->IsIncognitoProfile()),
      avatar_(new AvatarToolbarButton(browser, this)),
      browser_(browser) {
  PageActionIconContainerView::Params params;
  params.types_enabled = {
      PageActionIconType::kManagePasswords,
      PageActionIconType::kLocalCardMigration,
      PageActionIconType::kSaveCard,
  };
  params.browser = browser_;
  params.command_updater = browser_->command_controller();
  params.page_action_icon_delegate = this;
  params.button_observer = this;
  params.view_observer = this;
  page_action_icon_container_view_ =
      AddChildView(std::make_unique<PageActionIconContainerView>(params));

  avatar_->SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification::ForSizeRule(
                           views::MinimumFlexSizeRule::kScaleToMinimum,
                           views::MaximumFlexSizeRule::kPreferred));
  AddMainButton(avatar_);
}

ToolbarAccountIconContainerView::~ToolbarAccountIconContainerView() = default;

void ToolbarAccountIconContainerView::UpdateAllIcons() {
  page_action_icon_container_view_->SetIconColor(GetIconColor());
  page_action_icon_container_view_->UpdateAll();
  avatar_->UpdateIcon();
}

SkColor ToolbarAccountIconContainerView::GetPageActionInkDropColor() const {
  return GetToolbarInkDropBaseColor(this);
}

float ToolbarAccountIconContainerView::GetPageActionInkDropVisibleOpacity()
    const {
  return kToolbarInkDropVisibleOpacity;
}

content::WebContents*
ToolbarAccountIconContainerView::GetWebContentsForPageActionIconView() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

std::unique_ptr<views::Border>
ToolbarAccountIconContainerView::CreatePageActionIconBorder() const {
  // With this border, the icon will have the same ink drop shape as toolbar
  // buttons.
  return views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_LABEL_BUTTON));
}

void ToolbarAccountIconContainerView::OnThemeChanged() {
  // Update icon color.
  UpdateAllIcons();
}

const char* ToolbarAccountIconContainerView::GetClassName() const {
  return kToolbarAccountIconContainerViewClassName;
}

const views::View::Views& ToolbarAccountIconContainerView::GetChildren() const {
  return page_action_icon_container_view_->children();
}
