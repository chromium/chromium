// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

class Browser;
class OmniboxController;
class PermissionDashboardController;
class PermissionDashboardView;
class WebUIToolbarWebView;

// A LocationBar implementation using WebUI.
class WebUILocationBar : public LocationBar,
                         public ContentSettingImageViewDelegate {
 public:
  WebUILocationBar(Browser* browser, LocationBarView::Delegate* delegate);
  ~WebUILocationBar() override;

  void Init(WebUIToolbarWebView* toolbar_view);

  // LocationBar:
  void FocusLocation(bool is_user_initiated,
                     bool clear_focus_if_failed) override;
  void FocusSearch() override;
  void UpdateFocusBehavior(bool toolbar_visible) override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  OmniboxController* GetOmniboxController() override;
  bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) override;
  ChipController* GetChipController() override;
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  ui::TrackedElement* GetAnchorOrNull() override;
  Browser* GetBrowser() override;
  void OnChanged() override;
  void UpdateWithoutTabRestore() override;
  bool IsInitialized() const override;
  bool IsVisible() const override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  bool IsEditingOrEmpty() const override;
  void InvalidateLayout() override;
  gfx::Rect Bounds() const override;
  gfx::Size MinimumSize() const override;
  gfx::Size PreferredSize() const override;
  void Update(content::WebContents* contents) override;
  void ResetTabState(content::WebContents* contents) override;
  bool HasSecurityStateChanged() override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // ContentSettingImageViewDelegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

 private:
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<LocationBarView::Delegate> delegate_ = nullptr;
  raw_ptr<WebUIToolbarWebView> toolbar_view_ = nullptr;

  std::unique_ptr<PermissionDashboardController>
      permission_dashboard_controller_;
  raw_ptr<PermissionDashboardView> permission_dashboard_view_ = nullptr;

  std::unique_ptr<OmniboxController> omnibox_controller_;

  bool is_initialized_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_
