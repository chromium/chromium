// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/customize_chrome/side_panel_controller_android.h"

#include <memory>
#include <string>

#include "chrome/browser/android/thin_webview/tab_thin_web_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace customize_chrome {

SidePanelControllerAndroid::SidePanelControllerAndroid(tabs::TabInterface& tab)
    : SidePanelControllerBase(tab),
      web_contents_host_(
          std::make_unique<thin_webview::android::TabThinWebViewHost>(tab)) {}

SidePanelControllerAndroid::~SidePanelControllerAndroid() = default;

SidePanelNativeView SidePanelControllerAndroid::CreateCustomizeChromeView(
    SidePanelEntryScope& /*scope*/) {
  if (!web_contents_) {
    content::WebContents::CreateParams params(tab_->GetProfile());
    web_contents_ = content::WebContents::Create(params);
    // TODO(crbug.com/507919199): Load kChromeUICustomizeChromeSidePanelURL.
    web_contents_->GetController().LoadURL(GURL(chrome::kChromeUIVersionURL),
                                           content::Referrer(),
                                           ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                           /*extra_headers=*/std::string());
  }

  web_contents_host_->SetWebContents(web_contents_.get());
  auto j_view = web_contents_host_->GetView();
  return j_view ? std::make_unique<SidePanelNativeViewAndroid>(j_view)
                : nullptr;
}

}  // namespace customize_chrome
