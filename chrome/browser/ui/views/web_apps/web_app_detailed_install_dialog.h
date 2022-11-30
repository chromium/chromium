// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DETAILED_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DETAILED_INSTALL_DIALOG_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/dialog_model.h"

class PrefService;

namespace content {
class NavigationHandle;
class WebContents;
}

namespace feature_engagement {
class Tracker;
}

namespace web_app {

class WebAppDetailedInstallDialogDelegate
    : public ui::DialogModelDelegate,
      public content::WebContentsObserver {
 public:
  WebAppDetailedInstallDialogDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      chrome::AppInstallationAcceptanceCallback callback,
      chrome::PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker);

  ~WebAppDetailedInstallDialogDelegate() override;

  void OnAccept();
  void OnCancel();

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void CloseDialog();

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppInstallInfo> install_info_;
  chrome::AppInstallationAcceptanceCallback callback_;
  chrome::PwaInProductHelpState iph_state_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<feature_engagement::Tracker> tracker_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DETAILED_INSTALL_DIALOG_H_
