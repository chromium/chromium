// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_border.h"

namespace {

toolbar_ui_api::mojom::SecurityChipIcon GetMojoSecurityChipIcon(
    location_bar::SecurityChipIcon security_chip_icon) {
  switch (security_chip_icon) {
    case location_bar::SecurityChipIcon::kHttp:
      return toolbar_ui_api::mojom::SecurityChipIcon::kHttp;
    case location_bar::SecurityChipIcon::kSecurePageInfo:
      return toolbar_ui_api::mojom::SecurityChipIcon::kSecurePageInfo;
    case location_bar::SecurityChipIcon::kNotSecureWarning:
      return toolbar_ui_api::mojom::SecurityChipIcon::kNotSecureWarning;
    case location_bar::SecurityChipIcon::kDangerous:
      return toolbar_ui_api::mojom::SecurityChipIcon::kDangerous;
    case location_bar::SecurityChipIcon::kGoogleSuperG:
      return toolbar_ui_api::mojom::SecurityChipIcon::kGoogleSuperG;
    case location_bar::SecurityChipIcon::kGoogleGMonochrome:
      return toolbar_ui_api::mojom::SecurityChipIcon::kGoogleGMonochrome;
    case location_bar::SecurityChipIcon::kAddContext:
      return toolbar_ui_api::mojom::SecurityChipIcon::kAddContext;
  }
  NOTREACHED();
}

toolbar_ui_api::mojom::SecurityLevel GetMojoSecurityLevel(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
      return toolbar_ui_api::mojom::SecurityLevel::kNone;
    case security_state::SECURE:
      return toolbar_ui_api::mojom::SecurityLevel::kSecure;
    case security_state::DANGEROUS:
      return toolbar_ui_api::mojom::SecurityLevel::kDangerous;
    case security_state::WARNING:
      return toolbar_ui_api::mojom::SecurityLevel::kWarning;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace

WebUILocationBar::WebUILocationBar(Browser* browser,
                                   LocationBarView::Delegate* delegate)
    : LocationBar(browser ? browser->command_controller() : nullptr),
      browser_(browser),
      delegate_(delegate),
      content_setting_image_control_(this) {}

WebUILocationBar::~WebUILocationBar() = default;

void WebUILocationBar::Init(WebUIToolbarWebView* toolbar_view) {
  toolbar_view_ = toolbar_view;

  // TODO(crbug.com/474060773): Replace the View with a WebUI impl.
  permission_dashboard_view_ =
      toolbar_view->AddChildView(std::make_unique<PermissionDashboardView>());

  permission_dashboard_controller_ =
      std::make_unique<PermissionDashboardController>(
          /*location_bar=*/this,
          /*content_settings_image_delegate=*/this, permission_dashboard_view_);

  omnibox_controller_ =
      std::make_unique<OmniboxController>(std::make_unique<ChromeOmniboxClient>(
          /*location_bar=*/this, browser_, browser_->profile()));
  omnibox_view_ =
      std::make_unique<WebUIReadOnlyOmnibox>(omnibox_controller_.get(), *this);

  omnibox_popup_view_ = std::make_unique<OmniboxPopupViewWebUI>(
      /*omnibox_view=*/omnibox_view_.get(), omnibox_controller_.get(),
      /*location_bar=*/this, /*presenter_delegate=*/*this);

  content_setting_image_control_.Init();

  // Unretained is safe because `this` owns `moved_subscription_`.
  moved_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          ui::kElementBoundsChangedEvent, kLocationBarElementId,
          BrowserElements::From(browser_)->GetContext(),
          base::BindRepeating(&WebUILocationBar::OnMoved,
                              base::Unretained(this)));

  is_initialized_ = true;
}

void WebUILocationBar::PropagateOmniboxUpdate(
    toolbar_ui_api::mojom::OmniboxViewStatePtr omnibox_state) {
  toolbar_view_->OnOmniboxViewStateChanged(std::move(omnibox_state));
}

void WebUILocationBar::FocusLocation(bool is_user_initiated,
                                     bool clear_focus_if_failed) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::FocusSearch() {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateFocusBehavior(bool toolbar_visible) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::UpdateContentSettingsIcons() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  if (!toolbar_view_) {
    return;
  }
  toolbar_view_->OnContentSettingChanged(
      content_setting_image_control_.ProcessContentSettingState(web_contents));
}

void WebUILocationBar::SaveStateToContents(content::WebContents* contents) {
  omnibox_view_->SaveStateToTab(contents);
}

void WebUILocationBar::Revert() {
  omnibox_view_->RevertAll();
}

OmniboxView* WebUILocationBar::GetOmniboxView() {
  return omnibox_view_.get();
}

OmniboxController* WebUILocationBar::GetOmniboxController() {
  return omnibox_controller_.get();
}

bool WebUILocationBar::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

ChipController* WebUILocationBar::GetChipController() {
  return permission_dashboard_controller_->request_chip_controller();
}

content::WebContents* WebUILocationBar::GetWebContents() {
  return delegate_->GetWebContents();
}

LocationBarModel* WebUILocationBar::GetLocationBarModel() {
  return delegate_->GetLocationBarModel();
}

std::optional<bubble_anchor_util::AnchorConfiguration>
WebUILocationBar::GetChipAnchor() {
  if (auto* chip_controller = GetChipController()) {
    if (auto* chip = chip_controller->chip(); chip && chip->GetVisible()) {
      return {{chip->GetAnchor(),
               PermissionChipView::kPermissionRequestChipElementId,
               views::BubbleBorder::TOP_LEFT}};
    }
  }
  return std::nullopt;
}

ui::TrackedElement* WebUILocationBar::GetAnchorOrNull() {
  return BrowserElements::From(browser_)->GetElement(kLocationBarElementId);
}

Browser* WebUILocationBar::GetBrowser() {
  return browser_.get();
}

Profile* WebUILocationBar::GetProfile() {
  return browser_->profile();
}

void WebUILocationBar::OnChanged() {
  UpdateLhsChipsState();
}

void WebUILocationBar::UpdateWithoutTabRestore() {
  Update(nullptr);
}

bool WebUILocationBar::IsInitialized() const {
  return is_initialized_;
}

bool WebUILocationBar::IsVisible() const {
  return toolbar_view_ && toolbar_view_->GetVisible();
}

bool WebUILocationBar::IsDrawn() const {
  return toolbar_view_ && toolbar_view_->IsDrawn();
}

bool WebUILocationBar::IsFullscreen() const {
  return toolbar_view_ && toolbar_view_->GetWidget()->IsFullscreen();
}

bool WebUILocationBar::IsEditingOrEmpty() const {
  return omnibox_view_ && omnibox_view_->IsEditingOrEmpty();
}

void WebUILocationBar::InvalidateLayout() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebUILocationBar::OnChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

gfx::Rect WebUILocationBar::Bounds() const {
  gfx::Rect screen_rect = BoundsInScreen();
  if (!screen_rect.IsEmpty()) {
    return views::View::ConvertRectFromScreen(toolbar_view_, screen_rect);
  }
  return gfx::Rect();
}

gfx::Rect WebUILocationBar::BoundsInScreen() const {
  ui::TrackedElement* anchor =
      BrowserElements::From(browser_)->GetElement(kLocationBarElementId);
  return anchor ? anchor->GetScreenBounds() : gfx::Rect();
}

gfx::Size WebUILocationBar::MinimumSize() const {
  // TODO(crbug.com/474060468): Proper calculation.
  return gfx::Size(400, 34);
}

gfx::Size WebUILocationBar::PreferredSize() const {
  // TODO(crbug.com/474060468): Proper calculation.
  return gfx::Size(400, 34);
}

void WebUILocationBar::Update(content::WebContents* contents) {
  NOTIMPLEMENTED();  // Or rather needs a bunch more

  if (contents) {
    omnibox_view_->OnTabChanged(contents);
  } else {
    omnibox_view_->Update();
  }

  OnChanged();
}

void WebUILocationBar::UpdateLhsChipsState() {
  LocationBarModel* model = GetLocationBarModel();
  bool is_editing_or_empty = IsEditingOrEmpty();

  auto security_chip_icon = location_bar::GetSecurityChipIconEnum(
      model, /*is_add_context_button_shown=*/false);
  std::u16string security_chip_text = location_bar::GetSecurityChipText(
      model, GetWebContents(), is_editing_or_empty);
  bool is_clickable = location_bar::IsSecurityChipInteractive(
      is_editing_or_empty, security_chip_icon);

  auto mojo_security_chip_icon = GetMojoSecurityChipIcon(security_chip_icon);
  auto mojo_security_level = GetMojoSecurityLevel(model->GetSecurityLevel());

  bool is_text_dangerous =
      security_chip_text ==
      l10n_util::GetStringUTF16(IDS_DANGEROUS_VERBOSE_STATE);

  auto lhs_chips_state = toolbar_ui_api::mojom::LhsChipsState::New(
      toolbar_ui_api::mojom::SecurityChipState::New(
          mojo_security_chip_icon, mojo_security_level, security_chip_text,
          is_clickable, is_text_dangerous),
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>());

  if (toolbar_view_) {
    toolbar_view_->OnLhsChipsStateChanged(std::move(lhs_chips_state));
  }

  last_update_security_level_ = model->GetSecurityLevel();
}

void WebUILocationBar::ResetTabState(content::WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

bool WebUILocationBar::HasSecurityStateChanged() {
  return last_update_security_level_ !=
         GetLocationBarModel()->GetSecurityLevel();
}

LocationBarTesting* WebUILocationBar::GetLocationBarForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUILocationBar::OnLhsChipMousePressed(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::OnLhsChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::OnLhsChipExpandAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  NOTIMPLEMENTED();
}

void WebUILocationBar::OnLhsChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  NOTIMPLEMENTED();
}

bool WebUILocationBar::ShouldHideContentSettingImage() {
  if (omnibox_controller_->edit_model()->user_input_in_progress()) {
    return true;
  }
  return omnibox_controller_->IsPopupOpen();
}

content::WebContents* WebUILocationBar::GetContentSettingWebContents() {
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
WebUILocationBar::GetContentSettingBubbleModelDelegate() {
  return delegate_->GetContentSettingBubbleModelDelegate();
}

views::Widget* WebUILocationBar::GetLocationBarWidget() {
  return toolbar_view_->GetWidget();
}

OmniboxPopupFileSelector* WebUILocationBar::GetOmniboxPopupFileSelector()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

OmniboxPopupAimPresenter* WebUILocationBar::GetOmniboxPopupAimPresenter()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUILocationBar::OnMoved(ui::TrackedElement*) {
  NotifyBoundsChanged();
}
