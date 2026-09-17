// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_ANDROID_H_

#include <memory>

#include "chrome/browser/ui/customize_chrome/side_panel_controller_base.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace thin_webview::android {
class TabThinWebViewHost;
}

namespace customize_chrome {

class SidePanelControllerAndroid : public SidePanelControllerBase {
 public:
  explicit SidePanelControllerAndroid(tabs::TabInterface& tab);
  SidePanelControllerAndroid(const SidePanelControllerAndroid&) = delete;
  SidePanelControllerAndroid& operator=(const SidePanelControllerAndroid&) =
      delete;
  ~SidePanelControllerAndroid() override;

  content::WebContents* GetWebContentsForTesting() const {
    return web_contents_.get();
  }
  thin_webview::android::TabThinWebViewHost* GetWebContentsHostForTesting()
      const {
    return web_contents_host_.get();
  }

 private:
  SidePanelNativeView CreateCustomizeChromeView(
      SidePanelEntryScope& scope) override;

  // `web_contents_host_` holds a raw pointer to `web_contents_`, so it must be
  // declared last in order to be destroyed first.
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<thin_webview::android::TabThinWebViewHost> web_contents_host_;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_ANDROID_H_
