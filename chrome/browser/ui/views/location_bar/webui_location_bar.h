// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_

#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/mouse_constants.h"

class Browser;
class OmniboxController;
class OmniboxPopupView;
class OmniboxPopupViewWebUI;
class PermissionDashboardController;
class WebUIPermissionDashboard;
class Profile;
class WebUIToolbarControlDelegate;

// A LocationBar implementation using WebUI.
class WebUILocationBar : public LocationBar,
                         public ContentSettingImageViewDelegate,
                         public WebUIReadOnlyOmnibox::UpdatePropagator,
                         public OmniboxPopupPresenterDelegate {
 public:
  WebUILocationBar(Browser* browser, LocationBarView::Delegate* delegate);
  ~WebUILocationBar() override;

  void Init(WebUIToolbarControlDelegate* delegate);

  // WebUIReadOnlyOmnibox::UpdatePropagator:
  void PropagateOmniboxUpdate(
      toolbar_ui_api::mojom::OmniboxViewStatePtr update) override;
  void PropagateFocusRequest(
      toolbar_ui_api::mojom::FocusRequestTarget target) override;

  // Called from WebUIToolbarWebView:
  void OnThemeChanged();
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> OnOmniboxAction(
      toolbar_ui_api::mojom::OmniboxActionPtr action);

  // LocationBar:
  void FocusLocation(bool is_user_initiated,
                     bool clear_focus_if_failed) override;
  void FocusSearch() override;
  void UpdateFocusBehavior(bool toolbar_visible) override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  OmniboxPopupView* GetOmniboxPopupView() override;
  OmniboxController* GetOmniboxController() override;
  bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) override;
  ChipController* GetChipController() override;
  content::WebContents* GetWebContents() override;

  // LocationBarTesting:
  LocationBarModel* GetLocationBarModel() override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  ui::TrackedElement* GetAnchorOrNull() override;
  Browser* GetBrowser() override;
  Profile* GetProfile() override;
  void OnChanged() override;
  void UpdateWithoutTabRestore() override;
  bool IsInitialized() const override;
  bool IsVisible() const override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  bool IsEditingOrEmpty() const override;
  void InvalidateLayout() override;
  gfx::Rect Bounds() const override;
  gfx::Rect BoundsInScreen() const override;
  gfx::Size MinimumSize() const override;
  gfx::Size PreferredSize() const override;
  void Update(content::WebContents* contents) override;
  void ResetTabState(content::WebContents* contents) override;
  bool HasSecurityStateChanged() override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // Left hand side (LHS) chip events (called from WebUIToolbarWebView)
  void OnLhsChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier);
  void OnLhsChipClicked(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                        bool is_mouse_interaction);
  void OnLhsChipPointerEntered(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier);
  void OnLhsChipPointerExited(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier);
  void OnLhsChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier);
  void OnLhsChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier);
  void OnLhsChipDrag(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                     ui::mojom::DragEventSource source);

  WebUIContentSettingImageControl& content_setting_image_control() {
    return content_setting_image_control_;
  }

  // ContentSettingImageViewDelegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // OmniboxPopupPresenterDelegate:
  views::Widget* GetLocationBarWidget() override;
  OmniboxPopupFileSelector* GetOmniboxPopupFileSelector() const override;
  OmniboxPopupAimPresenter* GetOmniboxPopupAimPresenter() const override;

  void SetSuppressionThresholdForTesting(base::TimeDelta threshold);

  PermissionDashboardController* permission_dashboard_controller() {
    return permission_dashboard_controller_.get();
  }

 private:
  friend class WebUILocationBarTest;

  // Determines whether the location icon should be overridden while a chip is
  // being displayed.
  bool ShouldChipOverrideLocationIcon();
  bool ShouldShowAddContextButton();

  void OnMovedOrShown(ui::TrackedElement* element);

  void UpdateLocationBarFlagsState();
  void UpdateSelectedKeywordState();

  // Updates the state of the LHS location bar chips (e.g. security chip) and
  // pushes it to the WebUI.
  void UpdateLhsChipsState(bool icon_known = false);

  ui::ImageModel UpdateLocationIcon(
      toolbar_ui_api::mojom::SecurityLevel security_level,
      bool is_text_dangerous);

  void OnIconFetched(const gfx::Image& image);

  void OnPageInfoBubbleClosed(views::Widget::ClosedReason closed_reason,
                              bool reload_prompt);

  void ShowPageInfoBubble();

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<LocationBarView::Delegate> delegate_ = nullptr;
  raw_ptr<WebUIToolbarControlDelegate> toolbar_delegate_ = nullptr;

  ui::ElementTracker::Subscription moved_subscription_;
  ui::ElementTracker::Subscription shown_subscription_;

  // Must be declared before `permission_dashboard_controller_` because the
  // `permission_dashboard_controller_` depends on models owned by
  // `content_setting_image_control_` during teardown.
  WebUIContentSettingImageControl content_setting_image_control_;

  // Threshold for suppressing LHS chip clicks after bubble closing.
  base::TimeDelta suppression_threshold_ =
      views::kMinimumTimeBetweenButtonClicks;

  std::unique_ptr<WebUIPermissionDashboard> permission_dashboard_;
  std::unique_ptr<PermissionDashboardController>
      permission_dashboard_controller_;

  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<WebUIReadOnlyOmnibox> omnibox_view_;
  std::unique_ptr<OmniboxPopupViewWebUI> omnibox_popup_view_;

  bool is_initialized_ = false;

  toolbar_ui_api::IconHandle location_icon_;
  security_state::SecurityLevel last_update_security_level_ =
      security_state::NONE;

  base::TimeTicks last_page_info_bubble_close_time_;
  bool suppress_lhs_chip_clicked_ = false;

  base::WeakPtrFactory<WebUILocationBar> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_
