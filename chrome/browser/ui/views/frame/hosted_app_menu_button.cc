// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_menu_button.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/extensions/hosted_app_menu_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/window/hit_test_utils.h"

HostedAppMenuButton::HostedAppMenuButton(BrowserView* browser_view)
    : AppMenuButton(this), browser_view_(browser_view) {
  views::SetHitTestComponent(this, static_cast<int>(HTMENU));

  SetInkDropMode(InkDropMode::ON);
  // Disable focus ring for consistency with sibling buttons and AppMenuButton.
  SetFocusPainter(nullptr);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // Avoid the native theme border, which would crop the icon (see
  // https://crbug.com/831968).
  SetBorder(nullptr);
  // This name is guaranteed not to change during the lifetime of this button.
  // Get the app name only, aka "Google Docs" instead of "My Doc - Google Docs",
  // because the menu applies to the entire app.
  base::string16 app_name = base::UTF8ToUTF16(
      browser_view->browser()->hosted_app_controller()->GetAppShortName());
  SetAccessibleName(app_name);
  SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_HOSTED_APPMENU_TOOLTIP, app_name));
  int size = GetLayoutConstant(HOSTED_APP_MENU_BUTTON_SIZE);
  SetMinSize(gfx::Size(size, size));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

HostedAppMenuButton::~HostedAppMenuButton() {}

void HostedAppMenuButton::SetColor(SkColor color) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kBrowserToolsIcon, color));
  ink_drop_color_ = color;
}

void HostedAppMenuButton::StartHighlightAnimation() {
  GetInkDrop()->SetHoverHighlightFadeDurationMs(
      HostedAppButtonContainer::kOriginFadeInDuration.InMilliseconds());
  GetInkDrop()->SetHovered(true);
  GetInkDrop()->UseDefaultHoverHighlightFadeDuration();

  highlight_off_timer_.Start(FROM_HERE,
                             HostedAppButtonContainer::kOriginFadeInDuration +
                                 HostedAppButtonContainer::kOriginPauseDuration,
                             this, &HostedAppMenuButton::FadeHighlightOff);
}

void HostedAppMenuButton::OnMenuButtonClicked(views::MenuButton* source,
                                              const gfx::Point& point,
                                              const ui::Event* event) {
  Browser* browser = browser_view_->browser();
  InitMenu(std::make_unique<HostedAppMenuModel>(browser_view_, browser),
           browser, AppMenu::NO_FLAGS);

  menu()->RunMenu(this);

  // Add UMA for how many times the hosted app menu button are clicked.
  base::RecordAction(
      base::UserMetricsAction("HostedAppMenuButtonButton_Clicked"));
}

SkColor HostedAppMenuButton::GetInkDropBaseColor() const {
  return ink_drop_color_;
}

void HostedAppMenuButton::FadeHighlightOff() {
  if (!ShouldEnterHoveredState()) {
    GetInkDrop()->SetHoverHighlightFadeDurationMs(
        HostedAppButtonContainer::kOriginFadeOutDuration.InMilliseconds());
    GetInkDrop()->SetHovered(false);
    GetInkDrop()->UseDefaultHoverHighlightFadeDuration();
  }
}

const char* HostedAppMenuButton::GetClassName() const {
  return "HostedAppMenuButton";
}
