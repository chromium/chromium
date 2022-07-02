// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_utils.h"

#include <set>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

bool AddWebContentsToSet(std::set<content::WebContents*>* frame_set,
                         const std::string& web_view_name,
                         content::WebContents* web_contents) {
  auto* web_view = extensions::WebViewGuest::FromWebContents(web_contents);
  if (web_view && web_view->name() == web_view_name)
    frame_set->insert(web_contents);
  return false;
}

}  // namespace

namespace signin {

content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name) {
  content::WebContents* auth_web_contents =
      GetAuthFrameWebContents(web_contents, parent_frame_name);
  return auth_web_contents ? auth_web_contents->GetPrimaryMainFrame() : nullptr;
}

content::WebContents* GetAuthFrameWebContents(
    content::WebContents* web_contents,
    const std::string& parent_frame_name) {
  std::set<content::WebContents*> frame_set;
  auto* manager = guest_view::GuestViewManager::FromBrowserContext(
      web_contents->GetBrowserContext());
  if (manager) {
    manager->ForEachGuest(web_contents,
                          base::BindRepeating(&AddWebContentsToSet, &frame_set,
                                              parent_frame_name));
  }
  DCHECK_GE(1U, frame_set.size());
  if (!frame_set.empty())
    return *frame_set.begin();

  return nullptr;
}

Browser* GetDesktopBrowser(content::WebUI* web_ui) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui->GetWebContents());
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
