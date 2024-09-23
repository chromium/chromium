// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/hit_test_utils.h"

WebAppMenuButton::WebAppMenuButton(BrowserView* browser_view,
                                   std::u16string accessible_name)
    : AppMenuButton(base::BindRepeating(&WebAppMenuButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_view_(browser_view) {
  views::SetHitTestComponent(this, static_cast<int>(HTCLIENT));

  SetVectorIcons(kBrowserToolsIcon, kBrowserToolsTouchIcon);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Currently, |accessible_name| is ony set for custom tabs for ARC apps on
  // ChromeOS. Skip setting the tooltip because
  // |IDS_WEB_APP_MENU_BUTTON_TOOLTIP| doesn't make sense combined with
  // |accessible_name|.
  if (accessible_name.empty()) {
    DCHECK(browser_view->browser()->app_controller());
    std::u16string application_name =
        browser_view->browser()->app_controller()->GetAppShortName();

    accessible_name = l10n_util::GetStringFUTF16(
        IDS_WEB_APP_MENU_BUTTON_TOOLTIP, application_name);
    SetTooltipText(accessible_name);
  }

  GetViewAccessibility().SetName(accessible_name);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

WebAppMenuButton::~WebAppMenuButton() = default;

void WebAppMenuButton::StartHighlightAnimation() {
  views::InkDrop::Get(this)->GetInkDrop()->SetHoverHighlightFadeDuration(
      WebAppToolbarButtonContainer::kOriginFadeInDuration);
  views::InkDrop::Get(this)->GetInkDrop()->SetHovered(true);
  views::InkDrop::Get(this)
      ->GetInkDrop()
      ->UseDefaultHoverHighlightFadeDuration();

  highlight_off_timer_.Start(
      FROM_HERE,
      WebAppToolbarButtonContainer::kOriginFadeInDuration +
          WebAppToolbarButtonContainer::kOriginPauseDuration,
      this, &WebAppMenuButton::FadeHighlightOff);
}

void WebAppMenuButton::ButtonPressed(const ui::Event& event) {
  Browser* browser = browser_view_->browser();
  RunMenu(std::make_unique<WebAppMenuModel>(browser_view_, browser), browser,
          event.IsKeyEvent() ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
                             : views::MenuRunner::NO_FLAGS);

  // Add UMA for how many times the web app menu button are clicked.
  base::RecordAction(
      base::UserMetricsAction("HostedAppMenuButtonButton_Clicked"));
}

int WebAppMenuButton::GetIconSize() const {
  // Rather than use the default toolbar icon size, use whatever icon size is
  // embedded in the vector icon. This matches the behavior of
  // BrowserAppMenuButton.
  return 0;
}

void WebAppMenuButton::FadeHighlightOff() {
  if (!ShouldEnterHoveredState()) {
    views::InkDrop::Get(this)->GetInkDrop()->SetHoverHighlightFadeDuration(
        WebAppToolbarButtonContainer::kOriginFadeOutDuration);
    views::InkDrop::Get(this)->GetInkDrop()->SetHovered(false);
    views::InkDrop::Get(this)
        ->GetInkDrop()
        ->UseDefaultHoverHighlightFadeDuration();
  }
}

BEGIN_METADATA(WebAppMenuButton)
END_METADATA
