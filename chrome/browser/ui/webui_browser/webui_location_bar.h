// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_LOCATION_BAR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

class WebUIBrowserWindow;

// A LocationBar implementation for WebUIBrowser.
class WebUILocationBar : public LocationBar {
 public:
  explicit WebUILocationBar(WebUIBrowserWindow* window);
  ~WebUILocationBar() override;

  // LocationBar:
  void FocusLocation(bool is_user_initiated) override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  OmniboxController* GetOmniboxController() override;
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  void OnChanged() override;
  void UpdateWithoutTabRestore() override;
  LocationBarTesting* GetLocationBarForTesting() override;

 private:
  const raw_ptr<WebUIBrowserWindow> window_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_LOCATION_BAR_H_
