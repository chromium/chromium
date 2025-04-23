// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_PWA_INSTALL_PAGE_ACTION_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_PWA_INSTALL_PAGE_ACTION_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace page_actions {
class PageActionController;
}

namespace tabs {
class TabInterface;
}

// A plus icon to surface whether a site has passed PWA (progressive web app)
// installability checks and can be installed.
// Can't be copied nor assigned.
class PwaInstallPageActionController
    : public content::WebContentsObserver,
      public webapps::AppBannerManager::Observer {
 public:
  explicit PwaInstallPageActionController(tabs::TabInterface& tab_interface);
  ~PwaInstallPageActionController() override;

  PwaInstallPageActionController(const PwaInstallPageActionController&) =
      delete;
  PwaInstallPageActionController& operator=(
      const PwaInstallPageActionController&) = delete;

  // webapps::AppBannerManager::Observer:
  void OnInstallableWebAppStatusUpdated(
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data) override;

  // content::WebContentsObserver
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  // Handles all the logic related to showing and hiding the page action.
  void UpdateVisibility();

  // Returns the controller of all page actions.
  page_actions::PageActionController& GetPageActionController();

  // Requests PageActionController to show this Page Action
  void Show(content::WebContents* web_contents, bool showChip);
  // Requests PageActionController to hide this Page Action
  void Hide();

  raw_ptr<webapps::AppBannerManager> manager_;
  raw_ref<tabs::TabInterface> tab_interface_;

  void WillDiscardContents(tabs::TabInterface* tab_interface,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  base::CallbackListSubscription will_discard_contents_subscription_;
  void WillDeactivate(tabs::TabInterface* tab_interface);
  base::CallbackListSubscription will_deactivate_subscription_;

  base::WeakPtrFactory<PwaInstallPageActionController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_PWA_INSTALL_PAGE_ACTION_H_
