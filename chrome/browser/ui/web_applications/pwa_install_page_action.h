// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_PWA_INSTALL_PAGE_ACTION_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_PWA_INSTALL_PAGE_ACTION_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
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
      public webapps::AppBannerManager::Observer,
      public page_actions::PageActionObserver {
 public:
  explicit PwaInstallPageActionController(tabs::TabInterface& tab_interface);
  ~PwaInstallPageActionController() override;

  PwaInstallPageActionController(const PwaInstallPageActionController&) =
      delete;
  PwaInstallPageActionController& operator=(
      const PwaInstallPageActionController&) = delete;

  // Sets that the callback (i.e. the action) is being executed.
  void SetIsExecuting(bool);

  // webapps::AppBannerManager::Observer:
  void OnInstallableWebAppStatusUpdated(
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data) override;

  // content::WebContentsObserver
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // page_actions::PageActionObserver
  void OnPageActionIconShown(
      const page_actions::PageActionState& page_action) override;

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

  // Called when the IPH is shown.
  void OnIphShown(user_education::FeaturePromoResult result);

  // Called when IPH is closed.
  void OnIphClosed(const webapps::ManifestId manifest_id);

  // Whether the IPH feature is enabled.
  bool iph_is_enabled_ = false;

  // Whether the IPH is trying to show.
  // iph_pending_ is true if the iph has been queued to be shown.
  // If set to true, attempting to show another page
  // action will not update the params for the FeaturePromo.
  bool iph_pending_ = false;

  // Track whether the callback (i.e. the action) is being executed.
  bool is_executing_ = false;

  // Decide whether IPH promo should be shown based on previous interactions
  // and if IPH has already been requested to be shown.
  bool ShouldShowIph(content::WebContents* web_contents,
                     const webapps::WebAppBannerData& data);

  base::WeakPtrFactory<PwaInstallPageActionController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_PWA_INSTALL_PAGE_ACTION_H_
