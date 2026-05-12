// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/mouse_constants.h"

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
      content_setting_image_control_(this) {
  permission_dashboard_ = std::make_unique<WebUIPermissionDashboard>(this);
  permission_dashboard_controller_ =
      std::make_unique<PermissionDashboardController>(
          /*location_bar=*/this,
          /*content_settings_image_delegate=*/this,
          permission_dashboard_.get());
}

WebUILocationBar::~WebUILocationBar() = default;

void WebUILocationBar::Init(WebUIToolbarControlDelegate* delegate) {
  toolbar_delegate_ = delegate;

  omnibox_controller_ =
      std::make_unique<OmniboxController>(std::make_unique<ChromeOmniboxClient>(
          /*location_bar=*/this, browser_, browser_->profile()));
  omnibox_view_ =
      std::make_unique<WebUIReadOnlyOmnibox>(omnibox_controller_.get(), *this);

  omnibox_popup_view_ = std::make_unique<OmniboxPopupViewWebUI>(
      /*omnibox_view=*/omnibox_view_.get(), omnibox_controller_.get(),
      /*location_bar=*/this, /*presenter_delegate=*/*this);

  content_setting_image_control_.Init(delegate);

  // Unretained is safe because `this` owns `moved_subscription_`.
  moved_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          ui::kElementBoundsChangedEvent, kLocationBarElementId,
          BrowserElements::From(browser_)->GetContext(),
          base::BindRepeating(&WebUILocationBar::OnMovedOrShown,
                              base::Unretained(this)));
  shown_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kLocationBarElementId, BrowserElements::From(browser_)->GetContext(),
          base::BindRepeating(&WebUILocationBar::OnMovedOrShown,
                              base::Unretained(this)));

  is_initialized_ = true;
}

void WebUILocationBar::PropagateOmniboxUpdate(
    toolbar_ui_api::mojom::OmniboxViewStatePtr omnibox_state) {
  toolbar_delegate_->OnOmniboxViewStateChanged(std::move(omnibox_state));
}

void WebUILocationBar::OnOmniboxAction(
    toolbar_ui_api::mojom::OmniboxActionPtr action) {
  switch (action->which()) {
    case toolbar_ui_api::mojom::OmniboxAction::Tag::kFocusChange:
      OnOmniboxFocusChange(*action->get_focus_change());
      break;
    case toolbar_ui_api::mojom::OmniboxAction::Tag::kTextInput:
      OnOmniboxTextInput(*action->get_text_input());
      break;
    case toolbar_ui_api::mojom::OmniboxAction::Tag::kKey:
      OnOmniboxKey(*action->get_key());
      break;
  }

  UpdateLocationBarFlagsState();
}

void WebUILocationBar::OnOmniboxFocusChange(
    const toolbar_ui_api::mojom::OmniboxActionFocusChange& focus_change) {
  if (focus_change.has_focus) {
    // TODO(crbug.com/500653057): Key state, though Views impl doesn't have it.
    omnibox_controller_->edit_model()->OnSetFocus(/*control_down=*/false);
  } else {
    omnibox_controller_->edit_model()->OnKillFocus();
    if (auto* popup_closer =
            omnibox_controller_->client()->GetOmniboxPopupCloser()) {
      popup_closer->CloseWithReason(omnibox::PopupCloseReason::kBlur);
    }
  }
}

void WebUILocationBar::OnOmniboxTextInput(
    const toolbar_ui_api::mojom::OmniboxActionTextInput& text_input) {
  omnibox_view_->OnBeforePossibleChange();
  omnibox_view_->SetTextAndSelectedRange(text_input.text,
                                         text_input.inline_autocompletion,
                                         gfx::Range(text_input.text.size()));
  omnibox_view_->OnAfterPossibleChange(/*allow_keyword_ui_change=*/true);
}

void WebUILocationBar::OnOmniboxKey(
    const toolbar_ui_api::mojom::OmniboxActionKey& key) {
  // TODO(crbug.com/500653057): Handle modifier keys.
  // TODO(crbug.com/500653057): Convert to DomKey (with some caching
  // since the converter is slow) once the JS end is more selective about
  // what it sends.
  if (key.key == "Enter") {
    omnibox_controller_->edit_model()->OpenCurrentSelection(
        base::TimeTicks::Now(), WindowOpenDisposition::CURRENT_TAB,
        /*via_keyboard=*/true);
  } else if (key.key == "Escape") {
    omnibox_controller_->edit_model()->OnEscapeKeyPressed();
  } else if (key.key == "ArrowUp") {
    omnibox_controller_->edit_model()->OnUpOrDownPressed(false, false);
  } else if (key.key == "ArrowDown") {
    omnibox_controller_->edit_model()->OnUpOrDownPressed(true, false);
  }
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
  if (!toolbar_delegate_) {
    return;
  }
  toolbar_delegate_->OnContentSettingChanged(
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
  if (event->type() != ui::EventType::kMousePressed) {
    return false;
  }

  if (BoundsInScreen().Contains(event->root_location())) {
    return false;
  }

  auto* const view = static_cast<views::View*>(event->target());
  if (omnibox_popup_view_->presenter()->GetOuterView()->Contains(view)) {
    return false;
  }

  return true;
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
  return toolbar_delegate_ && toolbar_delegate_->GetView()->GetVisible();
}

bool WebUILocationBar::IsDrawn() const {
  return toolbar_delegate_ && toolbar_delegate_->GetView()->IsDrawn();
}

bool WebUILocationBar::IsFullscreen() const {
  return toolbar_delegate_ &&
         toolbar_delegate_->GetView()->GetWidget()->IsFullscreen();
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
    return views::View::ConvertRectFromScreen(toolbar_delegate_->GetView(),
                                              screen_rect);
  }
  return gfx::Rect();
}

gfx::Rect WebUILocationBar::BoundsInScreen() const {
  ui::TrackedElement* anchor =
      BrowserElements::From(browser_)->GetElement(kLocationBarElementId);
  // Fallback to our parent container's bounds if we haven't gotten ours
  // yet; this should be correct for vertical margin computation, and start
  // the popup creation with something reasonable.
  return anchor ? anchor->GetScreenBounds()
                : toolbar_delegate_->GetView()->GetBoundsInScreen();
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
  UpdateLocationBarFlagsState();
}

void WebUILocationBar::UpdateLhsChipsState() {
  if (GetLocationBarWidget() && GetLocationBarWidget()->IsClosed()) {
    return;
  }
  LocationBarModel* model = GetLocationBarModel();
  bool is_editing_or_empty = IsEditingOrEmpty();

  auto security_chip_icon = location_bar::GetSecurityChipIconEnum(
      model, /*is_add_context_button_shown=*/false);
  std::u16string security_chip_text = location_bar::GetSecurityChipText(
      model, GetWebContents(), is_editing_or_empty);
  // TODO(crbug.com/509938007): Disable the security chip when one of the
  // Google logo icons is shown.
  bool is_clickable = !is_editing_or_empty;

  auto mojo_security_chip_icon = GetMojoSecurityChipIcon(security_chip_icon);
  auto mojo_security_level = GetMojoSecurityLevel(model->GetSecurityLevel());

  bool is_text_dangerous =
      security_chip_text ==
      l10n_util::GetStringUTF16(IDS_DANGEROUS_VERBOSE_STATE);

  auto lhs_chips_state = toolbar_ui_api::mojom::LhsChipsState::New(
      toolbar_ui_api::mojom::SecurityChipState::New(
          mojo_security_chip_icon, mojo_security_level, security_chip_text,
          is_clickable, is_text_dangerous, !ShouldChipOverrideLocationIcon()),
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>(),
      permission_dashboard_->GetState());

  if (toolbar_delegate_) {
    toolbar_delegate_->OnLhsChipsStateChanged(std::move(lhs_chips_state));
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
  if (identifier == toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon) {
    // Determine if the Page Info bubble was dismissed by this exact mouse
    // press.
    // 1. If the bubble is STILL open when this IPC arrives, it's about to
    // close.
    // 2. If the bubble was already closed by the native OS due to focus loss
    //    milliseconds before this IPC arrived, we check the close time.
    // We use the native kMinimumTimeBetweenButtonClicks (100ms) to safely
    // bridge the asynchronous WebUI IPC gap without inventing magic numbers.
    //
    // Note: If the user mouses down and drags out without releasing, this
    // flag remains true. This is safe because it will be unconditionally
    // overwritten by the next OnLhsChipMousePressed IPC when they click again.
    suppress_lhs_chip_clicked_ =
        (PageInfoBubbleView::GetShownBubbleType() !=
         PageInfoBubbleView::BUBBLE_NONE) ||
        (base::TimeTicks::Now() - last_page_info_bubble_close_time_ <
         views::kMinimumTimeBetweenButtonClicks);
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnMousePressed();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnMousePressed();
  }
}

void WebUILocationBar::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  last_page_info_bubble_close_time_ = base::TimeTicks::Now();

  // TODO(crbug.com/495419742): If `reload_prompt` is true, and the user closed
  // the bubble by pressing ESC or clicking the Close button, we should
  // refocus the location bar so the user can tab into the "You should reload
  // this page" infobar rather than being dumped back out into a stale webpage.
  // See `LocationBarView::OnPageInfoBubbleClosed` for the implementation.
}

void WebUILocationBar::OnLhsChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_mouse_interaction) {
  if (identifier == toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon) {
    // Prevent reopening the bubble if it was just closed by this exact click.
    // We only suppress mouse interactions because keyboard activations (e.g.
    // pressing Enter) do not cause native focus loss and therefore don't suffer
    // from this race condition. This matches the native Views implementation in
    // IconLabelBubbleView::IsTriggerableEvent.
    if (is_mouse_interaction) {
      if (suppress_lhs_chip_clicked_) {
        suppress_lhs_chip_clicked_ = false;
        return;
      }
    }

    ShowPageInfoBubble();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnClicked();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnClicked();
  } else {
    NOTREACHED();
  }
}

void WebUILocationBar::ShowPageInfoBubble() {
  // WebContents can be null during window teardown/startup, or if the tab
  // crashed/closed while this asynchronous IPC was in flight. We return early
  // rather than CHECKing to avoid crashing the browser in these edge cases.
  content::WebContents* contents = GetWebContents();
  if (!contents) {
    return;
  }
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || entry->IsInitialEntry()) {
    return;
  }

  ui::TrackedElement* location_bar_element = GetAnchorOrNull();

  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          location_bar_element
              ? views::BubbleAnchor(location_bar_element)
              : views::BubbleAnchor(toolbar_delegate_->GetView()),
          toolbar_delegate_->GetView()->GetWidget()->GetNativeWindow(),
          contents, entry->GetVirtualURL())

          .AddPageInfoClosingCallback(
              base::BindOnce(&WebUILocationBar::OnPageInfoBubbleClosed,
                             weak_ptr_factory_.GetWeakPtr()))
          .Build();
  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
  bubble->SetHighlightedElement(kLocationIconElementId);
  bubble->GetWidget()->Show();
}

void WebUILocationBar::OnLhsChipPointerEntered(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (identifier ==
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnMouseEntered();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnMouseEntered();
  }
}

void WebUILocationBar::OnLhsChipPointerExited(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (identifier ==
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnMouseExited();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnMouseExited();
  }
}

void WebUILocationBar::OnLhsChipExpandAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (identifier ==
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnExpandAnimationEnded();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnExpandAnimationEnded();
  }
}

void WebUILocationBar::OnLhsChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (identifier ==
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnCollapseAnimationEnded();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnCollapseAnimationEnded();
  }
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
  return toolbar_delegate_->GetView()->GetWidget();
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

bool WebUILocationBar::ShouldChipOverrideLocationIcon() {
  return permission_dashboard_->GetIndicatorChip()->GetVisible() ||
         permission_dashboard_->GetRequestChip()->GetVisible();
}

void WebUILocationBar::OnMovedOrShown(ui::TrackedElement* element) {
  NotifyBoundsChanged();
}

void WebUILocationBar::UpdateLocationBarFlagsState() {
  auto location_bar_flags = toolbar_ui_api::mojom::LocationBarFlags::New();
  location_bar_flags->user_input_in_progress =
      omnibox_controller_->edit_model()->user_input_in_progress();
  location_bar_flags->popup_open = omnibox_controller_->IsPopupOpen();
  toolbar_delegate_->OnLocationBarFlagsChanged(std::move(location_bar_flags));
}
