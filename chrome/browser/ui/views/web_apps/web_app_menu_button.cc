// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/hit_test_utils.h"

WebAppMenuButton::WebAppMenuButton(BrowserView* browser_view)
    : AppMenuButton(this), browser_view_(browser_view) {
  views::SetHitTestComponent(this, static_cast<int>(HTMENU));

  SetInkDropMode(InkDropMode::ON);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // This name is guaranteed not to change during the lifetime of this button.
  // Get the app name only, aka "Google Docs" instead of "My Doc - Google Docs",
  // because the menu applies to the entire app.
  base::string16 app_name = base::UTF8ToUTF16(
      browser_view->browser()->app_controller()->GetAppShortName());
  SetAccessibleName(app_name);
  SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_WEB_APP_MENU_BUTTON_TOOLTIP, app_name));

  constexpr int focus_mode_app_menu_button_size = 34;
  bool is_focus_mode = browser_view->browser()->is_focus_mode();
  int size = is_focus_mode ? focus_mode_app_menu_button_size
                           : GetLayoutConstant(WEB_APP_MENU_BUTTON_SIZE);
  SetMinSize(gfx::Size(size, size));
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

WebAppMenuButton::~WebAppMenuButton() {}

void WebAppMenuButton::SetColor(SkColor color) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kBrowserToolsIcon, color));
  ink_drop_color_ = color;
}

void WebAppMenuButton::StartHighlightAnimation() {
  GetInkDrop()->SetHoverHighlightFadeDuration(
      WebAppFrameToolbarView::kOriginFadeInDuration);
  GetInkDrop()->SetHovered(true);
  GetInkDrop()->UseDefaultHoverHighlightFadeDuration();

  highlight_off_timer_.Start(FROM_HERE,
                             WebAppFrameToolbarView::kOriginFadeInDuration +
                                 WebAppFrameToolbarView::kOriginPauseDuration,
                             this, &WebAppMenuButton::FadeHighlightOff);
}

void WebAppMenuButton::ButtonPressed(views::Button* source,
                                     const ui::Event& event) {
  Browser* browser = browser_view_->browser();
  RunMenu(std::make_unique<WebAppMenuModel>(browser_view_, browser), browser,
          event.IsKeyEvent() ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
                             : views::MenuRunner::NO_FLAGS,
          false);

  // Add UMA for how many times the web app menu button are clicked.
  base::RecordAction(
      base::UserMetricsAction("HostedAppMenuButtonButton_Clicked"));
}

SkColor WebAppMenuButton::GetInkDropBaseColor() const {
  return ink_drop_color_;
}

void WebAppMenuButton::FadeHighlightOff() {
  if (!ShouldEnterHoveredState()) {
    GetInkDrop()->SetHoverHighlightFadeDuration(
        WebAppFrameToolbarView::kOriginFadeOutDuration);
    GetInkDrop()->SetHovered(false);
    GetInkDrop()->UseDefaultHoverHighlightFadeDuration();
  }
}

const char* WebAppMenuButton::GetClassName() const {
  return "WebAppMenuButton";
}
