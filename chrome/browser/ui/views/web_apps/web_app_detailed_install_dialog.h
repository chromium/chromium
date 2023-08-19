// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DETAILED_INSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DETAILED_INSTALL_DIALOG_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

namespace webapps {
class MlInstallOperationTracker;
}  // namespace webapps

namespace web_app {

class WebAppDetailedInstallDialogDelegate
    : public ui::DialogModelDelegate,
      public content::WebContentsObserver {
 public:
  WebAppDetailedInstallDialogDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      chrome::AppInstallationAcceptanceCallback callback,
      chrome::PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker);

  ~WebAppDetailedInstallDialogDelegate() override;

  void OnAccept();
  void OnCancel();
  void OnClose();

  base::WeakPtr<WebAppDetailedInstallDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void CloseDialogAsIgnored();
  void MeasureIphOnDialogClose();

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppInstallInfo> install_info_;
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker_;
  chrome::AppInstallationAcceptanceCallback callback_;
  chrome::PwaInProductHelpState iph_state_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<feature_engagement::Tracker> tracker_;

  base::WeakPtrFactory<WebAppDetailedInstallDialogDelegate> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DETAILED_INSTALL_DIALOG_H_
