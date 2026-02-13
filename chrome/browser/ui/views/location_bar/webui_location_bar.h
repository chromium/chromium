// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

namespace chrome {
class BrowserCommandController;
}  // namespace chrome
class WebUIToolbarWebView;

// A LocationBar implementation using WebUI.
class WebUILocationBar : public LocationBar {
 public:
  WebUILocationBar(chrome::BrowserCommandController* command_controller,
                   WebUIToolbarWebView* toolbar_view);
  ~WebUILocationBar() override;

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
  bool IsVisible() const override;
  bool IsDrawn() const override;
  bool IsTopLevelFullscreen() const override;
  bool IsEditingOrEmpty() const override;
  void InvalidateLayout() override;
  gfx::Rect Bounds() const override;
  gfx::Size MinimumSize() const override;
  gfx::Size PreferredSize() const override;
  void Update(content::WebContents* contents) override;
  void ResetTabState(content::WebContents* contents) override;
  bool HasSecurityStateChanged() override;
  LocationBarTesting* GetLocationBarForTesting() override;

 private:
  const raw_ptr<WebUIToolbarWebView> toolbar_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_LOCATION_BAR_H_
