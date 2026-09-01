// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_actions.h"
#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_factory.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/theme_resources.h"
#endif

namespace {

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

WebUILocationBar::WebUILocationBar(BrowserWindowInterface* browser,
                                   LocationBarView::Delegate* delegate)
    : LocationBar(browser ? chrome::BrowserCommandController::From(browser)
                          : nullptr),
      browser_(browser),
      delegate_(delegate),
      content_setting_image_control_(this),
      page_action_control_(
          browser ? BrowserActions::From(browser)->root_action_item()
                  : nullptr),
      using_full_popup_(
          base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup)) {
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
          /*location_bar=*/this, browser_, browser_->GetProfile()));
  omnibox_view_ = std::make_unique<WebUIReadOnlyOmnibox>(
      /*location_bar=*/this, toolbar_delegate_, omnibox_controller_.get(),
      /*update_propagator=*/*this);

  if (using_full_popup_) {
    omnibox_popup_view_ = std::make_unique<OmniboxPopupViewFullWebUI>(
        /*omnibox_view=*/omnibox_view_.get(),
        /*controller=*/omnibox_controller_.get(), /*location_bar=*/this,
        /*presenter_delegate=*/*this);
  } else {
    omnibox_popup_view_ = std::make_unique<OmniboxPopupViewWebUI>(
        /*omnibox_view=*/omnibox_view_.get(), omnibox_controller_.get(),
        /*location_bar=*/this, /*presenter_delegate=*/*this);
  }

  // This location bar implementation isn't used with web apps or devtools
  // windows as of now. If this changes, we will need to be careful to not
  // create extra processes for the popups in cases where they can't actually
  // be shown.
  const bool is_web_app =
      browser_ && web_app::AppBrowserController::IsWebApp(browser_);
  const bool is_devtools =
      browser_ &&
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS;
  DCHECK(!is_web_app);
  DCHECK(!is_devtools);

  if (omnibox::IsAimPopupFeatureEnabled()) {
    omnibox_popup_aim_presenter_ = std::make_unique<OmniboxPopupAimPresenter>(
        /*location_bar=*/this, omnibox_controller_.get(),
        /*presenter_delegate=*/*this);
    omnibox_popup_file_selector_ = std::make_unique<OmniboxPopupFileSelector>(
        GetLocationBarWidget()->GetNativeWindow());
  }

  content_setting_image_control_.Init(delegate);
  page_action_control_.Init(delegate);

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

  // Watch popup state to help switch classical <-> AIM.
  popup_state_changed_subscription_ =
      omnibox_controller_->popup_state_manager()->AddPopupStateChangedCallback(
          base::BindRepeating(&WebUILocationBar::OnPopupStateChanged,
                              base::Unretained(this)));

  RegisterOmniboxActions(browser_);

  is_initialized_ = true;
}

void WebUILocationBar::PropagateOmniboxUpdate(
    toolbar_ui_api::mojom::OmniboxViewStatePtr omnibox_state) {
  // `toolbar_delegate_` is null in some tests.
  if (toolbar_delegate_) {
    toolbar_delegate_->OnOmniboxViewStateChanged(std::move(omnibox_state));
  }
}

void WebUILocationBar::PropagateApplyFocusRingToAimButton(bool force_focus) {
  force_aim_button_focus_ring_ = force_focus;
  UpdateLocationBarFlagsState();
}

void WebUILocationBar::PropagateFocusRequest(
    toolbar_ui_api::mojom::FocusRequestTarget target) {
  // TODO(crbug.com/503784990): Handle immersive lock; this is tricky since
  // our focus request is async. Compare OmniboxViewViews::SetFocus.
  // `toolbar_delegate_` is null in some tests.

  // In case of full popup, we want to hand over control to it immediately,
  // so do what OmniboxViewViews would, rather than going to our WebUI.
  if (using_full_popup_) {
    // ... well, almost immediately, since we may be in middle of activation
    // (see views::Widget::Activate()), so trying to activate the popup instead
    // can make things very unhappy.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebUILocationBar::HandleFocusRequestForFullPopup,
                       weak_ptr_factory_.GetWeakPtr(), target));
  } else if (toolbar_delegate_) {
    toolbar_delegate_->OnFocusRequested(target);
  }
}

void WebUILocationBar::OpenOmniboxIfFullPopup(bool query_zps) {
  if (using_full_popup_ && !in_popup_state_transition_) {
    omnibox_popup_view_->OnFocus(query_zps);
  }
}

void WebUILocationBar::OnThemeChanged() {
  if (!is_initialized_) {
    return;
  }
  // Location icon cares about color scheme.
  UpdateLhsChipsState();
  UpdatePageActions(/*contents=*/nullptr);
}

void WebUILocationBar::HandleContextMenu(
    views::Widget* widget,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    const content::ContextMenuParams& menu_params) {
  omnibox_view_->HandleContextMenu(widget, point, source_type, menu_params);
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUILocationBar::OnOmniboxAction(
    toolbar_ui_api::mojom::OmniboxActionPtr action) {
  auto result = omnibox_view_->OnOmniboxAction(std::move(action));
  UpdateLocationBarFlagsState();
  UpdateSelectedKeywordState();
  return result;
}

void WebUILocationBar::SetFocusWithin(bool focused) {
  focus_within_ = focused;

  // Focus state affects whether AI mode button is visible or not.
  RefreshAiModePageAction();

  NotifyFocusChanged();
}

void WebUILocationBar::OnBlur() {
  SetFocusWithin(false);
  if (omnibox_view_) {
    omnibox_view_->OnBlur();
  }
}

void WebUILocationBar::FocusLocation(bool is_user_initiated,
                                     bool clear_focus_if_failed) {
  omnibox_view_->SetFocus(is_user_initiated);
}

void WebUILocationBar::FocusSearch() {
  omnibox_view_->SetFocusWithTarget(
      toolbar_ui_api::mojom::FocusRequestTarget::kSearch);
}

void WebUILocationBar::UpdateFocusBehavior(bool toolbar_visible) {
  // It doesn't seem like we need to do any adjustment, if the toolbar is
  // invisible the right thing should happen already.
}

void WebUILocationBar::UpdateContentSettingsIcons() {
  // If the LHS permission chip models changed visibility or state, propagate
  // the updated LHS dashboard state to the WebUI.
  if (UpdateContentSettingModels()) {
    UpdateLhsChipsState();
  }
}

bool WebUILocationBar::UpdateContentSettingModels() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }

  bool permission_dashboard_changed = false;
  bool dashboard_updated = false;

  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    ContentSettingImageModel* media_stream_model =
        content_setting_image_control_.GetModel(
            ContentSettingImageModel::ImageType::kMediaStream);
    if (media_stream_model) {
      permission_dashboard_changed |=
          permission_dashboard_controller_->Update(media_stream_model);
      if (media_stream_model->is_visible()) {
        dashboard_updated = true;
      }
    }
  }

  if (!dashboard_updated &&
      base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideSensorActivityIndicators)) {
    ContentSettingImageModel* sensors_model =
        content_setting_image_control_.GetModel(
            ContentSettingImageModel::ImageType::kSensors);
    if (sensors_model) {
      permission_dashboard_changed |=
          permission_dashboard_controller_->Update(sensors_model);
    }
  }

  if (!toolbar_delegate_) {
    return permission_dashboard_changed;
  }
  toolbar_delegate_->OnContentSettingChanged(
      content_setting_image_control_.ProcessContentSettingState(web_contents));

  return permission_dashboard_changed;
}

void WebUILocationBar::SaveStateToContents(content::WebContents* contents) {
  if (using_full_popup_) {
    // We're counting on full popup saving the same state format.
    omnibox_popup_view_->SaveStateToTab(contents);
  } else {
    omnibox_view_->SaveStateToTab(contents);
  }
}

void WebUILocationBar::Revert() {
  omnibox_view_->RevertAll();
  if (using_full_popup_ && !in_popup_state_transition_) {
    omnibox_controller_->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

OmniboxView* WebUILocationBar::GetOmniboxView() {
  return omnibox_view_.get();
}

OmniboxPopupPresenterDelegate* WebUILocationBar::GetPresenterDelegate() {
  return this;
}

OmniboxPopupView* WebUILocationBar::GetOmniboxPopupView() {
  return omnibox_popup_view_.get();
}

OmniboxController* WebUILocationBar::GetOmniboxController() {
  return omnibox_controller_.get();
}

bool WebUILocationBar::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed) {
    return false;
  }

  auto* const view = static_cast<views::View*>(event->target());
  auto event_coords =
      views::View::ConvertPointToScreen(view, event->location());
  if (BoundsInScreen().Contains(event_coords)) {
    return false;
  }

  if (omnibox_popup_view_->presenter()->GetOuterView()->Contains(view)) {
    return false;
  }

  return true;
}

ChipController* WebUILocationBar::GetChipController() {
  return permission_dashboard_controller_->request_chip_controller();
}

PermissionDashboardController*
WebUILocationBar::GetPermissionDashboardController() {
  return permission_dashboard_controller_.get();
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

bool WebUILocationBar::in_popup_state_transition() const {
  return in_popup_state_transition_;
}

BrowserWindowInterface* WebUILocationBar::GetBrowser() {
  return browser_.get();
}

Profile* WebUILocationBar::GetProfile() {
  return browser_->GetProfile();
}

void WebUILocationBar::OnChanged() {
  UpdateLhsChipsState();
  UpdateLocationBarFlagsState();
  UpdateSelectedKeywordState();
  RefreshAiModePageAction();
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

bool WebUILocationBar::IsMouseHovered() const {
  return IsVisible() && BoundsInScreen().Contains(
                            display::Screen::Get()->GetCursorScreenPoint());
}

bool WebUILocationBar::IsFocusWithin() const {
  return focus_within_;
}

void WebUILocationBar::InvalidateLayout() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebUILocationBar::OnChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

gfx::Rect WebUILocationBar::Bounds() const {
  if (!toolbar_delegate_) {
    return gfx::Rect();
  }
  gfx::Rect screen_rect = BoundsInScreen();
  if (!screen_rect.IsEmpty()) {
    return views::View::ConvertRectFromScreen(toolbar_delegate_->GetView(),
                                              screen_rect);
  }
  return gfx::Rect();
}

gfx::Rect WebUILocationBar::BoundsInScreen() const {
  if (!toolbar_delegate_) {
    return gfx::Rect();
  }
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
  return gfx::Size(300, 34);
}

gfx::Size WebUILocationBar::PreferredSize() const {
  // TODO(crbug.com/474060468): Proper calculation.
  return gfx::Size(400, 34);
}

void WebUILocationBar::Update(content::WebContents* contents) {
  if (contents) {
    omnibox_view_->OnTabChanged(contents);
    if (using_full_popup_) {
      omnibox_popup_view_->OnTabChanged(contents);
    }
  } else {
    omnibox_view_->Update();
  }

  UpdateContentSettingModels();
  UpdatePageActions(contents);
  OnChanged();
}

void WebUILocationBar::UpdateLhsChipsState(bool icon_known) {
  if (!toolbar_delegate_) {
    return;
  }
  if (GetLocationBarWidget() && GetLocationBarWidget()->IsClosed()) {
    return;
  }
  LocationBarModel* model = GetLocationBarModel();
  bool is_editing_or_empty = IsEditingOrEmpty();

  std::u16string security_chip_text = location_bar::GetSecurityChipText(
      model, GetWebContents(), is_editing_or_empty);
  bool is_clickable = !is_editing_or_empty;

  auto mojo_security_level = GetMojoSecurityLevel(model->GetSecurityLevel());

  bool is_text_dangerous =
      security_chip_text ==
      l10n_util::GetStringUTF16(IDS_DANGEROUS_VERBOSE_STATE);

  // `omnibox_view_` is null in some tests.
  if (!icon_known && omnibox_view_) {
    ui::ImageModel maybe_new_icon =
        UpdateLocationIcon(mojo_security_level, is_text_dangerous);
    if (!maybe_new_icon.IsEmpty()) {
      bool icon_handled = false;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      if (auto maybe_resource_id =
              location_bar::MaybeGetGradientGoogleSuperGIcon(maybe_new_icon)) {
        if (*maybe_resource_id == IDR_GOOGLE_G_GRADIENT_16_ALT) {
          location_icon_ = toolbar_delegate_->GetIconTable().RegisterColorUrl(
              "chrome://theme/IDR_GOOGLE_G_GRADIENT_16_ALT");
        } else {
          DCHECK_EQ(*maybe_resource_id, IDR_GOOGLE_G_GRADIENT_20);
          location_icon_ = toolbar_delegate_->GetIconTable().RegisterColorUrl(
              "chrome://theme/IDR_GOOGLE_G_GRADIENT_20");
        }
        // Google logo icons aren't clickable.
        is_clickable = false;
        icon_handled = true;
      }
#endif
      if (!icon_handled) {
        location_icon_ =
            toolbar_delegate_->GetIconTable().RegisterImageModelTryReuse(
                maybe_new_icon, location_icon_);
      }
    }
  }

  if (is_editing_or_empty &&
      (!ShouldShowPermissionPromptEvenIfOmniboxEditedOrEmpty(
           GetWebContents()) ||
       omnibox_controller_->IsPopupOpen())) {
    // Permission requests get cancelled if user edits the URL.
    // (And won't show up if it was already edited when they occurred).
    bool has_visible_chip = GetChipController()->chip()->GetVisible();
    bool has_permission_prompt =
        GetChipController()->active_permission_request_manager().has_value() &&
        GetChipController()
            ->active_permission_request_manager()
            .value()
            ->GetCurrentPrompt();

    if (has_visible_chip || has_permission_prompt) {
      // If a user starts typing, a permission request should be ignored and the
      // chip finalized.
      GetChipController()->ResetPermissionPromptChip();
    }
  }

  auto accessibility_state = location_bar::GetSecurityChipAccessibilityState(
      model, is_editing_or_empty, security_chip_text);

  const bool is_context_menu_visible =
      PageInfoBubbleView::GetShownBubbleType() !=
      PageInfoBubbleView::BUBBLE_NONE;

  auto lhs_chips_state = toolbar_ui_api::mojom::LhsChipsState::New(
      toolbar_ui_api::mojom::SecurityChipState::New(
          location_icon_, mojo_security_level, security_chip_text,
          location_bar::GetSecurityChipTooltipText(is_editing_or_empty),
          toolbar_ui_api::mojom::SecurityChipAccessibilityState::New(
              accessibility_state.role == ax::mojom::Role::kImage
                  ? toolbar_ui_api::mojom::SecurityChipRole::kImage
                  : toolbar_ui_api::mojom::SecurityChipRole::kButton,
              accessibility_state.name, accessibility_state.description),
          is_clickable, is_text_dangerous, !ShouldChipOverrideLocationIcon(),
          is_context_menu_visible),
      std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>(),
      permission_dashboard_->GetState());

  if (toolbar_delegate_) {
    toolbar_delegate_->OnLhsChipsStateChanged(std::move(lhs_chips_state));
  }

  last_update_security_level_ = model->GetSecurityLevel();
}

void WebUILocationBar::UpdatePageActions(content::WebContents* contents) {
  content::WebContents* active_contents = contents;
  if (!active_contents && browser_) {
    active_contents = browser_->GetTabStripModel()->GetActiveWebContents();
  }
  page_action_control_.UpdateController(active_contents);
  page_action_control_.SetShouldHidePageActions(ShouldHideRHSIcons());
}

ui::ImageModel WebUILocationBar::UpdateLocationIcon(
    toolbar_ui_api::mojom::SecurityLevel security_level,
    bool is_text_dangerous) {
  // TODO(crbug.com/505362587): This duplicates quite some color logic
  // with the JS side, and also quite a bit of LocationBarView's logic.
  auto* color_provider = toolbar_delegate_->GetView()->GetColorProvider();

  const ui::ColorId background_id =
      is_text_dangerous ? kColorOmniboxSecurityChipDangerousBackground
                        : kColorOmniboxIconBackground;

  bool dark_mode = color_utils::IsDark(color_provider->GetColor(background_id));

  ui::ColorId id = kColorOmniboxText;
  if (security_level == toolbar_ui_api::mojom::SecurityLevel::kDangerous) {
    id = kColorOmniboxSecurityChipDangerous;
  }
  if (is_text_dangerous) {
    id = kColorOmniboxSecurityChipText;
  }

  const int dip_size =
      GetLayoutConstant(LayoutConstant::kLocationBarLeadingIconSize);

  return omnibox_view_->GetIcon(
      dip_size, color_provider->GetColor(id),
      color_provider->GetColor(kColorOmniboxResultsIcon),
      color_provider->GetColor(kColorOmniboxResultsStarterPackIcon),
      color_provider->GetColor(kColorOmniboxAnswerIconGM3Foreground),
      base::BindOnce(&WebUILocationBar::OnIconFetched,
                     weak_ptr_factory_.GetWeakPtr()),
      dark_mode);
}

void WebUILocationBar::OnIconFetched(const gfx::Image& image) {
  location_icon_ = toolbar_delegate_->GetIconTable().RegisterImageModel(
      ui::ImageModel::FromImage(image));
  UpdateLhsChipsState(/*icon_known=*/true);
}

void WebUILocationBar::ResetTabState(content::WebContents* contents) {
  omnibox_view_->ResetTabState(contents);
}

bool WebUILocationBar::HasSecurityStateChanged() {
  return last_update_security_level_ !=
         GetLocationBarModel()->GetSecurityLevel();
}

LocationBarTesting* WebUILocationBar::GetLocationBarForTesting() {
  return this;
}

bool WebUILocationBar::TestContentSettingImagePressed(size_t index) {
  return content_setting_image_control_.TestPressed(index);
}

bool WebUILocationBar::IsContentSettingBubbleShowing(size_t index) {
  return content_setting_image_control_.IsBubbleShowing(index);
}

void WebUILocationBar::OnLhsChipMousePressed(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_middle_click) {
  if (identifier == toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon) {
    if (location_bar::InitiateMiddleClickPasteIfSupported(
            is_middle_click,
            base::BindOnce(&WebUILocationBar::OnMiddleClickPaste,
                           weak_ptr_factory_.GetWeakPtr(),
                           base::TimeTicks::Now()))) {
      return;
    }

    page_info_reopen_suppressor_.OnMousePressed();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnMousePressed();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnMousePressed();
  }
}

void WebUILocationBar::OnLhsChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_mouse_interaction) {
  if (identifier == toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon) {
    // Prevent reopening the bubble if it was just closed by this exact click.
    if (page_info_reopen_suppressor_.ShouldSuppressBubbleShow(
            is_mouse_interaction)) {
      return;
    }

    ShowPageInfoBubble();
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator) {
    permission_dashboard_->indicator_chip()->OnClicked(is_mouse_interaction);
  } else if (identifier ==
             toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest) {
    permission_dashboard_->request_chip()->OnClicked(is_mouse_interaction);
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

  ui::TrackedElement* anchor_element =
      BrowserElements::From(browser_)->GetElement(kLocationIconElementId);
  if (!anchor_element) {
    anchor_element = GetAnchorOrNull();
  }

  base::OnceClosure initialized_callback =
      GetPageInfoDialogCreatedCallbackForTesting()                   // IN-TEST
          ? std::move(GetPageInfoDialogCreatedCallbackForTesting())  // IN-TEST
          : base::DoNothing();

  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          anchor_element ? views::BubbleAnchor(anchor_element)
                         : views::BubbleAnchor(toolbar_delegate_->GetView()),
          toolbar_delegate_->GetView()->GetWidget()->GetNativeWindow(),
          contents, entry->GetVirtualURL())
          .AddInitializedCallback(std::move(initialized_callback))
          .AddPageInfoClosingCallback(
              base::BindOnce(&WebUILocationBar::OnPageInfoBubbleClosed,
                             weak_ptr_factory_.GetWeakPtr()))
          .Build();
  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
  bubble->SetHighlightedElement(kLocationIconElementId);
  bubble->GetWidget()->Show();
  page_info_reopen_suppressor_.Observe(bubble->GetWidget());
  UpdateLhsChipsState();
}

void WebUILocationBar::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  UpdateLhsChipsState();

  if (!reload_prompt) {
    return;
  }
  if (closed_reason != views::Widget::ClosedReason::kEscKeyPressed &&
      closed_reason != views::Widget::ClosedReason::kCloseButtonClicked) {
    return;
  }

  // Refocus the location bar if a page reload is required and the user closed
  // the bubble via ESC key or close button. This allows the user to easily tab
  // into the reload infobar.
  FocusLocation(/*is_user_initiated=*/false, /*clear_focus_if_failed=*/false);
}

void WebUILocationBar::HandleFocusRequestForFullPopup(
    toolbar_ui_api::mojom::FocusRequestTarget target) {
  // Of things handled here, only kLocationBar is not user-inititiated.
  const bool is_user_initiated =
      (target != toolbar_ui_api::mojom::FocusRequestTarget::kLocationBar);
  const bool omnibox_already_focused =
      omnibox_view_->has_focus() ||
      static_cast<OmniboxPopupViewFullWebUI*>(omnibox_popup_view_.get())
          ->is_focused();

  if (is_user_initiated) {
    // TODO(crbug.com/546101626): this seems to fail sometimes because of
    // incorrectly set user_input_in_progress() bit.
    omnibox_controller_->edit_model()->Unelide();
  }

  if (omnibox_already_focused) {
    omnibox_controller_->edit_model()->ClearKeyword();
  }

  // See comments in OmniboxViewViews::SetFocus.
  if (is_user_initiated || !omnibox_already_focused) {
    omnibox_view_->SelectAll(true);
  }

  if (target == toolbar_ui_api::mojom::FocusRequestTarget::kSearch) {
    omnibox_view_->EnterKeywordModeForDefaultSearchProvider();
  }

  omnibox_popup_view_->OnFocus(is_user_initiated);

  // TODO(crbug.com/546101626): This is a bit off; there is risk of races
  // (but that's true overall), and sometimes this side doesn't know about
  // unelide results.
  omnibox_popup_view_->SyncNativeStateToWebUI(is_user_initiated);
}

void WebUILocationBar::SetSuppressionThresholdForTesting(
    base::TimeDelta threshold) {
  page_info_reopen_suppressor_.SetSuppressionThresholdForTesting(  // IN-TEST
      threshold);
  page_action_control_.SetSuppressionThresholdForTesting(threshold);  // IN-TEST
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

void WebUILocationBar::OnLhsChipDrag(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    ui::mojom::DragEventSource source) {
  if (identifier != toolbar_ui_api::mojom::LhsChipIdentifier::kLocationIcon) {
    return;
  }

  content::WebContents* web_contents = GetWebContents();
  if (!web_contents || !web_contents->GetVisibleURL().is_valid() ||
      IsEditingOrEmpty()) {
    return;
  }

  auto data = std::make_unique<ui::OSExchangeData>();
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  gfx::ImageSkia favicon = favicon_driver->GetFavicon().AsImageSkia();

  button_drag_utils::SetURLAndDragImage(web_contents->GetVisibleURL(),
                                        web_contents->GetTitle(), favicon,
                                        /*press_pt=*/nullptr, data.get());

  int allowed_operations =
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;

  gfx::Point widget_point = display::Screen::Get()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(GetLocationBarWidget()->GetRootView(),
                                      &widget_point);

  GetLocationBarWidget()->RunDragDropLoop(toolbar_delegate_->GetView(),
                                          std::move(data), widget_point,
                                          allowed_operations, source);
}

void WebUILocationBar::AnnounceAlert(const std::u16string& announcement) {
  if (toolbar_delegate_) {
    toolbar_delegate_->AnnounceAlert(announcement);
  }
}

bool WebUILocationBar::ShouldHideContentSettingImage() {
  return ShouldHideRHSIcons();
}

content::WebContents* WebUILocationBar::GetContentSettingWebContents() {
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
WebUILocationBar::GetContentSettingBubbleModelDelegate() {
  return delegate_->GetContentSettingBubbleModelDelegate();
}

views::Widget* WebUILocationBar::GetLocationBarWidget() {
  return toolbar_delegate_ ? toolbar_delegate_->GetView()->GetWidget()
                           : nullptr;
}

OmniboxPopupFileSelector* WebUILocationBar::GetOmniboxPopupFileSelector()
    const {
  return omnibox_popup_file_selector_.get();
}

OmniboxPopupAimPresenter* WebUILocationBar::GetOmniboxPopupAimPresenter()
    const {
  return omnibox_popup_aim_presenter_.get();
}

views::View* WebUILocationBar::GetLocationBarFocusRestoreView() {
  return toolbar_delegate_ ? toolbar_delegate_->GetInternalWebView() : nullptr;
}

bool WebUILocationBar::ShouldChipOverrideLocationIcon() {
  return permission_dashboard_->GetIndicatorChip()->GetVisible() ||
         permission_dashboard_->GetRequestChip()->GetVisible();
}

bool WebUILocationBar::ShouldHideRHSIcons() {
  // When the user is typing in the omnibox, the page action icons are no longer
  // associated with the current omnibox text, so hide them.
  if (omnibox_controller_->edit_model()->user_input_in_progress()) {
    return true;
  }

  // Also hide them if the popup is open for any other reason, e.g. ZeroSuggest.
  // The page action icons are not relevant to the displayed suggestions.
  return omnibox_controller_->IsPopupOpen();
}

void WebUILocationBar::OnMovedOrShown(ui::TrackedElement* element) {
  NotifyBoundsChanged();
}

void WebUILocationBar::OnPopupStateChanged(OmniboxPopupState old_state,
                                           OmniboxPopupState new_state) {
  in_popup_state_transition_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebUILocationBar::ClearInPopupStateTransition,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(100));

  if (browser_ && base::FeatureList::IsEnabled(
                      features::kGlicHandoffButtonHideWhenOmniboxPopupOpened)) {
    if (auto* window_controller = ActorUiWindowController::From(browser_)) {
      window_controller->OnOmniboxPopupStateChanged(new_state !=
                                                    OmniboxPopupState::kNone);
    }
  }

  if (new_state != OmniboxPopupState::kNone) {
    // Close any overlapping user education bubbles when any popup opens.
    // It's not great for promos to overlap the omnibox if the user opens the
    // drop-down after showing the promo. This especially causes issues on Mac
    // and Linux due to z-order/rendering issues, see crbug.com/40775593 and
    // crbug.com/332769403 for examples.
    BrowserHelpBubble::MaybeCloseOverlappingHelpBubbles(browser_,
                                                        BoundsInScreen());
  }

  // Hide the old popup.
  switch (old_state) {
    case OmniboxPopupState::kClassic:
      // Normally, the classic/full popup hides itself in
      // `UpdatePopupAppearance()` before updating the popup state. However,
      // explicitly hide the classic/full popup for scenario of transitioning
      // from the classic/full to the aim popup.
      if (omnibox_popup_view_->IsOpen()) {
        omnibox_popup_view_->UpdatePopupAppearance();
      }
      break;
    case OmniboxPopupState::kFull:
      if (omnibox_popup_view_->presenter()) {
        omnibox_popup_view_->presenter()->Hide();
      }
      break;
    case OmniboxPopupState::kAim:
      if (omnibox_popup_aim_presenter_) {
        omnibox_popup_aim_presenter_->Hide();
      }
      break;
    case OmniboxPopupState::kNone:
      break;
  }

  // Show the new popup.
  switch (new_state) {
    case OmniboxPopupState::kClassic:
      // The classic/full popup shows itself in `UpdatePopupAppearance()` before
      // updating the popup state.
      break;
    case OmniboxPopupState::kFull:
      if (omnibox_popup_view_->presenter()) {
        omnibox_popup_view_->presenter()->Show();
      }
      break;
    case OmniboxPopupState::kAim:
      if (omnibox_popup_aim_presenter_) {
        omnibox_popup_aim_presenter_->Show();
      }
      break;
    case OmniboxPopupState::kNone:
      break;
  }

  UpdateWithoutTabRestore();
}

void WebUILocationBar::ClearInPopupStateTransition() {
  in_popup_state_transition_ = false;
  // AIM Placeholder text gets deferred during transition if
  // kOmniboxAimDeferShowUntilVisualStateReady is on,
  // so request a repaint when the transition period expires.
  if (omnibox_view_ &&
      base::FeatureList::IsEnabled(
          omnibox::kOmniboxAimDeferShowUntilVisualStateReady)) {
    omnibox_view_->RequestUpdateWebUI();
  }
}

// If omnibox is open, notify Omnibox presenter that a permission prompt is
// starting right before constructing the prompt view widget. This is the
// notification point that is before and closest to when the view is rendered,
// which ensures the omnibox knows as soon as possible and ignores focus-loss
// events during the whole time that the embedded permission prompt is showing.
void WebUILocationBar::SetPermissionPromptShowing(bool showing) {
  OmniboxPopupPresenterBase* presenter = nullptr;
  // Get Omnibox popup presenter for AIM or normal omnibox, depending
  // on which is showing.
  if (omnibox_popup_aim_presenter_ && omnibox_popup_aim_presenter_->IsShown()) {
    presenter = omnibox_popup_aim_presenter_.get();
  } else if (GetOmniboxPopupView() && GetOmniboxPopupView()->presenter() &&
             GetOmniboxPopupView()->presenter()->IsShown()) {
    presenter = GetOmniboxPopupView()->presenter();
  }
  if (presenter) {
    presenter->SetPermissionPromptShowing(showing);
  }
}

void WebUILocationBar::UpdateLocationBarFlagsState() {
  if (!toolbar_delegate_ || !omnibox_controller_) {  // null in some tests.
    return;
  }

  auto location_bar_flags = toolbar_ui_api::mojom::LocationBarFlags::New();
  location_bar_flags->user_input_in_progress =
      omnibox_controller_->edit_model()->user_input_in_progress();
  location_bar_flags->popup_open = omnibox_controller_->IsPopupOpen();
  location_bar_flags->force_aim_button_focus_ring =
      force_aim_button_focus_ring_;
  views::Widget* widget = toolbar_delegate_->GetView()->GetWidget();
  location_bar_flags->is_virtual_keyboard_visible =
      widget && LocationBarView::IsVirtualKeyboardVisible(widget);
  toolbar_delegate_->OnLocationBarFlagsChanged(std::move(location_bar_flags));
}

void WebUILocationBar::UpdateSelectedKeywordState() {
  if (!toolbar_delegate_ || !omnibox_controller_) {  // null in some tests.
    return;
  }

  const std::u16string& current_keyword =
      omnibox_controller_->edit_model()->keyword();
  bool is_keyword_selected =
      omnibox_controller_->edit_model()->is_keyword_selected();
  if (last_search_keyword_ == current_keyword &&
      last_is_keyword_selected_ == is_keyword_selected) {
    // Avoid recomputing this state, especially icon.
    return;
  }
  last_search_keyword_ = current_keyword;
  last_is_keyword_selected_ = is_keyword_selected;

  Profile* profile = browser_->GetProfile();

  // Purposefully start with null here.
  toolbar_ui_api::mojom::SelectedKeywordStatePtr keyword_state;
  if (is_keyword_selected) {
    keyword_state = toolbar_ui_api::mojom::SelectedKeywordState::New();

    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    SelectedKeywordView::KeywordLabelNames keyword_labels =
        SelectedKeywordView::GetKeywordLabelNames(current_keyword,
                                                  template_url_service);
    keyword_state->short_name = keyword_labels.short_name;
    keyword_state->full_name = keyword_labels.full_name;

    keyword_icon_ =
        toolbar_delegate_->GetIconTable().RegisterImageModelTryReuse(
            SelectedKeywordView::GetKeywordIcon(
                current_keyword, omnibox_controller_.get(), profile),
            keyword_icon_);
    keyword_state->icon = keyword_icon_;
  }
  toolbar_delegate_->OnSelectedKeywordChanged(std::move(keyword_state));
}

void WebUILocationBar::RefreshAiModePageAction() {
  auto* aim_page_action_controller =
      omnibox::AiModePageActionController::From(browser_);
  if (aim_page_action_controller) {
    aim_page_action_controller->UpdatePageAction();
  }

  // TODO(crbug.com/491707187): kShowRhsAimHint support, if relevant.
}

void WebUILocationBar::OnMiddleClickPaste(base::TimeTicks event_timestamp,
                                          std::u16string text) {
  if (!omnibox_controller_) {
    return;
  }
  location_bar::ExecutePasteAndGo(
      *omnibox_controller_,
      AutocompleteClassifierFactory::GetForProfile(GetProfile()), text,
      event_timestamp);
}
