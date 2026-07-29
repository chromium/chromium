// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_STUB_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_STUB_LOCATION_BAR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

class BrowserWindowInterface;
class PermissionDashboardController;
class WebUIPermissionDashboard;
class WebUIBrowserWindow;

// A LocationBar implementation for WebUIBrowser.
class WebUIStubLocationBar : public LocationBar,
                             public ContentSettingImageViewDelegate {
 public:
  explicit WebUIStubLocationBar(WebUIBrowserWindow* window);
  ~WebUIStubLocationBar() override;

  // ContentSettingImageViewDelegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

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
  LocationBarModel* GetLocationBarModel() override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  ui::TrackedElement* GetAnchorOrNull() override;
  BrowserWindowInterface* GetBrowser() override;
  Profile* GetProfile() override;
  void OnChanged() override;
  void UpdateWithoutTabRestore() override;
  bool IsInitialized() const override;
  bool IsVisible() const override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  bool IsEditingOrEmpty() const override;
  bool IsMouseHovered() const override;
  bool IsFocusWithin() const override;
  void InvalidateLayout() override;
  gfx::Rect Bounds() const override;
  gfx::Rect BoundsInScreen() const override;
  gfx::Size MinimumSize() const override;
  gfx::Size PreferredSize() const override;
  void Update(content::WebContents* contents) override;
  void ResetTabState(content::WebContents* contents) override;
  bool HasSecurityStateChanged() override;
  LocationBarTesting* GetLocationBarForTesting() override;
  void AnnounceAlert(const std::u16string& announcement) override;

 private:
  const raw_ptr<WebUIBrowserWindow> window_;
  std::unique_ptr<WebUIPermissionDashboard> permission_dashboard_;
  std::unique_ptr<PermissionDashboardController>
      permission_dashboard_controller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_STUB_LOCATION_BAR_H_
