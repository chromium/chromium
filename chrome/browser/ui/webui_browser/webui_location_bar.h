// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_LOCATION_BAR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

class BrowserWindowInterface;

// A LocationBar implementation for WebUIBrowser.
class WebUILocationBar : public LocationBar {
 public:
  explicit WebUILocationBar(BrowserWindowInterface* browser);
  ~WebUILocationBar() override;

  // LocationBar:
  void FocusLocation(bool is_user_initiated) override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  void OnChanged() override;
  void OnPopupVisibilityChanged() override;
  void UpdateWithoutTabRestore() override;
  LocationBarTesting* GetLocationBarForTesting() override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_LOCATION_BAR_H_
