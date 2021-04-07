// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"

ExtensionsMenuButton::ExtensionsMenuButton(
    Browser* browser,
    ToolbarActionViewController* controller,
    bool allow_pinning)
    : HoverButton(base::BindRepeating(&ExtensionsMenuButton::ButtonPressed,
                                      base::Unretained(this)),
                  std::u16string()),
      browser_(browser),
      controller_(controller),
      allow_pinning_(allow_pinning) {
  ConfigureBubbleMenuItem(this, 0);
  controller_->SetDelegate(this);
  UpdateState();
}

ExtensionsMenuButton::~ExtensionsMenuButton() = default;

SkColor ExtensionsMenuButton::GetInkDropBaseColor() const {
  return HoverButton::GetInkDropColor(this);
}

bool ExtensionsMenuButton::CanShowIconInToolbar() const {
  return allow_pinning_;
}

// ToolbarActionViewDelegateViews:
views::View* ExtensionsMenuButton::GetAsView() {
  return this;
}

views::FocusManager* ExtensionsMenuButton::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Button* ExtensionsMenuButton::GetReferenceButtonForPopup() {
  return BrowserView::GetBrowserViewForBrowser(browser_)
      ->toolbar()
      ->GetExtensionsButton();
}

content::WebContents* ExtensionsMenuButton::GetCurrentWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ExtensionsMenuButton::UpdateState() {
  SetImage(
      Button::STATE_NORMAL,
      controller_
          ->GetIcon(GetCurrentWebContents(), ExtensionsMenuItemView::kIconSize)
          .AsImageSkia());
  SetText(controller_->GetActionName());
  SetTooltipText(controller_->GetTooltip(GetCurrentWebContents()));
  SetEnabled(controller_->IsEnabled(GetCurrentWebContents()));
  // The horizontal insets reasonably align the extension icons with text inside
  // the dialog. Note that |kIconSize| also contains space for badging, so we
  // can't trivially use dialog-text insets (empty space inside the icon).
  constexpr gfx::Insets kBorderInsets =
      gfx::Insets((ExtensionsMenuItemView::kMenuItemHeightDp -
                   ExtensionsMenuItemView::kIconSize.height()) /
                      2,
                  12);
  SetBorder(views::CreateEmptyBorder(kBorderInsets));
}

void ExtensionsMenuButton::ButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.ExtensionActivatedFromMenu"));
  controller_->ExecuteAction(
      true, ToolbarActionViewController::InvocationSource::kMenuEntry);
}

BEGIN_METADATA(ExtensionsMenuButton, views::LabelButton)
END_METADATA
