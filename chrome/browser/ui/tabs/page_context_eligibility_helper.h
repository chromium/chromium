// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PAGE_CONTEXT_ELIGIBILITY_HELPER_H_
#define CHROME_BROWSER_UI_TABS_PAGE_CONTEXT_ELIGIBILITY_HELPER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {
class PageContextEligibilityObserver;
class PageContextEligibility;
}  // namespace optimization_guide

namespace tabs {
class TabInterface;

// Tab-scoped helper class that wraps the PageContextEligibilityObserver.
// Provides a unified, easy-to-use API for other tab features to query and
// observe page context eligibility of active tabs.
class PageContextEligibilityHelper : public tabs::ContentsObservingTabFeature {
 public:
  explicit PageContextEligibilityHelper(tabs::TabInterface& tab);
  PageContextEligibilityHelper(const PageContextEligibilityHelper&) = delete;
  PageContextEligibilityHelper& operator=(const PageContextEligibilityHelper&) =
      delete;
  ~PageContextEligibilityHelper() override;

  DECLARE_USER_DATA(PageContextEligibilityHelper);

  static PageContextEligibilityHelper* From(tabs::TabInterface* tab);

  // Returns the current page context eligibility status of the active tab.
  // Returns kUnknown until loaded, or if the tab is inactive.
  virtual optimization_guide::PageContextEligibilityStatus
  IsPageContextEligible() const;

  using EligibilityChangeCallback = base::RepeatingCallback<void(
      optimization_guide::PageContextEligibilityStatus)>;

  // Registers a callback to be notified when eligibility changes.
  base::CallbackListSubscription RegisterEligibilityChangeCallback(
      EligibilityChangeCallback callback);

  friend class PageContextEligibilityHelperTest;

 protected:
  // tabs::ContentsObservingTabFeature:
  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

 private:
  void OnTabActivated(tabs::TabInterface* tab);
  void OnTabDeactivated(tabs::TabInterface* tab);
  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason);
  void OnEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus status);
  std::string GetAccountEmail();

  void CreateObserver(content::WebContents* contents);
  void ResetObserverAndNotifyUnknown();
  void NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus status);

  base::RepeatingCallbackList<void(
      optimization_guide::PageContextEligibilityStatus)>
      callbacks_;

  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  std::unique_ptr<optimization_guide::PageContextEligibilityObserver> observer_;

  optimization_guide::PageContextEligibilityStatus last_eligibility_ =
      optimization_guide::PageContextEligibilityStatus::kUnknown;

  ui::ScopedUnownedUserData<PageContextEligibilityHelper>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<PageContextEligibilityHelper> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PAGE_CONTEXT_ELIGIBILITY_HELPER_H_
