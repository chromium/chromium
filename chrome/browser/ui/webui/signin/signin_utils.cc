// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace signin {

content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name) {
  content::RenderFrameHost* frame = nullptr;
  web_contents->ForEachRenderFrameHostWithAction(
      [&frame, &parent_frame_name](content::RenderFrameHost* rfh) {
        auto* web_view = extensions::WebViewGuest::FromRenderFrameHost(rfh);
        if (web_view && web_view->name() == parent_frame_name) {
          DCHECK_EQ(web_view->GetGuestMainFrame(), rfh);
          frame = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return frame;
}

extensions::WebViewGuest* GetAuthWebViewGuest(
    content::WebContents* web_contents,
    const std::string& parent_frame_name) {
  return extensions::WebViewGuest::FromRenderFrameHost(
      GetAuthFrame(web_contents, parent_frame_name));
}

Browser* GetDesktopBrowser(content::WebUI* web_ui) {
  Browser* browser = chrome::FindBrowserWithTab(web_ui->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui));
  return browser;
}

void SetInitializedModalHeight(Browser* browser,
                               content::WebUI* web_ui,
                               const base::Value::List& args) {
  if (!browser)
    return;

  double height = args[0].GetDouble();
  browser->signin_view_controller()->SetModalSigninHeight(
      static_cast<int>(height));
}

}  // namespace signin
