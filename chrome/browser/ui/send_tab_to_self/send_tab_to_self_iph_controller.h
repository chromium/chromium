// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IPH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace send_tab_to_self {

// Controller responsible for showing the Send Tab To Self Tutorial IPH promo
// when a user action (navigation or tab switch) lands the active tab on an
// eligible page, once User Education session policy permits.
class SendTabToSelfIphController : public TabStripModelObserver,
                                   public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(SendTabToSelfIphController);
  explicit SendTabToSelfIphController(BrowserWindowInterface* interface);
  ~SendTabToSelfIphController() override;

  SendTabToSelfIphController(const SendTabToSelfIphController&) = delete;
  SendTabToSelfIphController& operator=(const SendTabToSelfIphController&) =
      delete;

  static SendTabToSelfIphController* From(BrowserWindowInterface* interface);

  // Returns true if `tab` is the tab most recently chosen as the promo anchor.
  bool IsPromoTab(tabs::TabHandle tab) const;

  // Records `promo_tab` as the promo anchor. Called by the promo's anchor
  // filter, which may run more than once before the promo shows.
  void SetPromoTab(tabs::TabHandle promo_tab);

  // Returns true if the Send Tab to Self entry point for `tab` would offer the
  // feature, i.e. GetEntryPointDisplayReason() returns kOfferFeature.
  static bool IsTabEligible(const tabs::TabInterface* tab);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void MaybeShowPromo();
  void OnShowPromoResult(user_education::FeaturePromoResult result);
  void StopObserving();

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ui::ScopedUnownedUserData<SendTabToSelfIphController> scoped_data_;
  bool promo_request_pending_ = false;
  tabs::TabHandle promo_tab_;
  base::WeakPtrFactory<SendTabToSelfIphController> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IPH_CONTROLLER_H_
