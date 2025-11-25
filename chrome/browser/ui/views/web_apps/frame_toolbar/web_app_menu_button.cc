// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/hit_test_utils.h"

WebAppMenuButton::WebAppMenuButton(BrowserView* browser_view)
    : AppMenuButton(base::BindRepeating(&WebAppMenuButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_view_(browser_view) {
  views::SetHitTestComponent(this, static_cast<int>(HTCLIENT));

  SetVectorIcons(kBrowserToolsIcon, kBrowserToolsTouchIcon);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetImageLabelSpacing(2);

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(browser_view->GetProfile());
  if (provider) {
    registrar_observation_.Observe(&provider->registrar_unsafe());
  }
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
  ShowMenu(event.IsKeyEvent() ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
                              : views::MenuRunner::NO_FLAGS);
  // Add UMA for how many times the web app menu button are clicked.
  base::RecordAction(
      base::UserMetricsAction("HostedAppMenuButtonButton_Clicked"));
}

bool WebAppMenuButton::IsLabelPresentAndVisible() const {
  if (!label()) {
    return false;
  }
  return label()->GetVisible() && !label()->GetText().empty();
}

void WebAppMenuButton::OnWebAppPendingUpdateChanged(
    const webapps::AppId& app_id,
    bool has_pending_update) {
  web_app::AppBrowserController* app_controller =
      browser_view_->browser()->app_controller();
  // `app_controller` can be null if this button is used in a (Chrome OS) custom
  // tab bar view for an ARC app.
  if (!app_controller) {
    return;
  }
  if (app_id == app_controller->app_id()) {
    UpdateTextAndHighlightColor(has_pending_update);
  }
}

void WebAppMenuButton::OnAppRegistrarDestroyed() {
  registrar_observation_.Observe(nullptr);
}

void WebAppMenuButton::UpdateStateForTesting() {
  UpdateTextAndHighlightColor(CanShowPendingUpdate());
}

base::CallbackListSubscription WebAppMenuButton::AwaitLabelTextUpdated(
    base::RepeatingClosure callback) {
  return label()->AddTextChangedCallback(callback);
}

void WebAppMenuButton::ShowMenu(int run_types) {
  Browser* browser = browser_view_->browser();
  RunMenu(std::make_unique<WebAppMenuModel>(browser_view_, browser), browser,
          run_types);
}

void WebAppMenuButton::OnThemeChanged() {
  UpdateTextAndHighlightColor(CanShowPendingUpdate());
  AppMenuButton::OnThemeChanged();
}

std::optional<SkColor> WebAppMenuButton::GetHighlightTextColor() const {
  if (!IsLabelPresentAndVisible()) {
    return std::nullopt;
  }
  return GetColorProvider()->GetColor(kColorAppMenuExpandedForegroundDefault);
}

SkColor WebAppMenuButton::GetForegroundColor(ButtonState state) const {
  if (IsLabelPresentAndVisible()) {
    return GetColorProvider()->GetColor(kColorAppMenuExpandedForegroundDefault);
  }
  return AppMenuButton::GetForegroundColor(state);
}

int WebAppMenuButton::GetIconSize() const {
  // Rather than use the default toolbar icon size, use whatever icon size is
  // embedded in the vector icon. This matches the behavior of
  // BrowserAppMenuButton.
  return 0;
}

std::optional<std::u16string> WebAppMenuButton::GetAccessibleNameOverride()
    const {
  return std::nullopt;
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

bool WebAppMenuButton::CanShowPendingUpdate() {
  web_app::AppBrowserController* app_controller =
      browser_view_->browser()->app_controller();
  // `app_controller` can be null if this button is used in a (Chrome OS) custom
  // tab bar view for an ARC app.
  return app_controller && app_controller->HasPendingUpdateNotIgnoredByUser();
}

void WebAppMenuButton::UpdateTextAndHighlightColor(bool is_pending_update) {
  web_app::AppBrowserController* app_controller =
      browser_view_->browser()->app_controller();
  // `app_controller` can be null if this button is used in a (Chrome OS) custom
  // tab bar view for an ARC app.

  int tooltip_message_id;
  std::u16string text;
  if (is_pending_update) {
    tooltip_message_id = IDS_WEB_APP_MENU_BUTTON_TOOLTIP_UPDATE_AVAILABLE;
    text = l10n_util::GetStringUTF16(IDS_WEB_APP_MENU_BUTTON_UPDATE);
  } else {
    tooltip_message_id = IDS_WEB_APP_MENU_BUTTON_TOOLTIP;
  }

  const bool label_present_and_visible = !text.empty();
  SetHorizontalAlignment(label_present_and_visible ? gfx::ALIGN_RIGHT
                                                   : gfx::ALIGN_CENTER);

  const auto* const color_provider = GetColorProvider();
  SetHighlight(text, label_present_and_visible
                         ? std::optional(color_provider->GetColor(
                               kColorAppMenuHighlightDefault))
                         : std::nullopt);

  SetLayoutInsets(label_present_and_visible
                      ? ::GetLayoutInsets(WEB_APP_APP_MENU_CHIP_PADDING)
                      : gfx::Insets());

  // In general all controls in the title bar of a web app should show up as
  // disabled in an inactive widget, so to fit in we also do this for the app
  // menu button. However setting the field to true when we have both a label
  // and an icon results in icon and label using different colors. For now not
  // supporting this behavior when the button is highlighted is easier (visually
  // this also seems fine, since when highlighted the app menu button is a
  // different color from all the other buttons anyway).
  SetAppearDisabledInInactiveWidget(!label_present_and_visible);

  // Custom tabs for ARC apps on ChromeOS use the same WebAppMenuButton, but
  // override the accessible name of the button to make more sense in that
  // context. In that case skip setting the tooltip because
  // `IDS_WEB_APP_MENU_BUTTON_TOOLTIP` doesn't make sense combined with
  // the overridden accessible name.
  std::optional<std::u16string> accessible_name = GetAccessibleNameOverride();
  if (!accessible_name.has_value()) {
    CHECK(app_controller);
    std::u16string application_name = app_controller->GetAppShortName();
    accessible_name =
        l10n_util::GetStringFUTF16(tooltip_message_id, application_name);
    SetTooltipText(*accessible_name);
  }
  GetViewAccessibility().SetName(*accessible_name);
}

BEGIN_METADATA(WebAppMenuButton)
END_METADATA
