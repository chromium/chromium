// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_BACK_TO_OPENER_BACK_TO_OPENER_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_BACK_TO_OPENER_BACK_TO_OPENER_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}

namespace back_to_opener {

class BackToOpenerController;

// Observer for the opener WebContents to detect when the opener relationship
// should be cleared. Monitors the opener tab for:
// - Destruction: Clears the relationship when the opener tab is closed
// - Navigation away: Clears the relationship when the opener navigates away
//   from its original URL (the URL it had when the relationship was
//   established)
class OpenerWebContentsObserver : public content::WebContentsObserver {
 public:
  OpenerWebContentsObserver(content::WebContents* opener,
                            base::WeakPtr<BackToOpenerController> controller);
  ~OpenerWebContentsObserver() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  base::WeakPtr<BackToOpenerController> controller_;
};

// Observes the WebContents to measure tab close duration and activate the
// opener tab once the tab is actually destroyed.
//
// ClosePage() doesn't immediately destroy the WebContents. The close process
// may involve unload prompts, and destruction happens asynchronously. This
// separate observer ensures we can reliably record metrics and activate the
// opener when the WebContents is actually destroyed.
class TabCloseObserver : public content::WebContentsObserver,
                         public content::WebContentsUserData<TabCloseObserver> {
 public:
  static TabCloseObserver* CreateForWebContents(
      content::WebContents* web_contents,
      base::WeakPtr<content::WebContents> opener_web_contents,
      base::TimeTicks close_start_time);
  ~TabCloseObserver() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<TabCloseObserver>;

  TabCloseObserver(content::WebContents* web_contents,
                   base::WeakPtr<content::WebContents> opener_web_contents,
                   base::TimeTicks close_start_time);

  base::TimeTicks close_start_time_;
  base::WeakPtr<content::WebContents> opener_web_contents_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Tab-scoped controller for managing back-to-opener functionality.
//
// An opener tab is the tab that opened this tab (the destination tab). The
// opener relationship is established when a navigation is initiated from one
// tab to create another tab, detected via the navigation's initiator frame.
//
// The opener relationship is maintained when:
// - The opener tab exists and has not navigated away from its original URL
// - The destination tab is not pinned (though the relationship is preserved
//   in case the tab is unpinned later)
//
// The opener relationship is lost when:
// - The opener tab is destroyed
// - The opener tab navigates away from the original URL it had when the
//   relationship was established
class BackToOpenerController : public tabs::ContentsObservingTabFeature {
 public:
  explicit BackToOpenerController(tabs::TabInterface& tab);
  BackToOpenerController(const BackToOpenerController&) = delete;
  BackToOpenerController& operator=(const BackToOpenerController&) = delete;
  ~BackToOpenerController() override;

  DECLARE_USER_DATA(BackToOpenerController);

  static const BackToOpenerController* From(const tabs::TabInterface* tab);
  static BackToOpenerController* From(tabs::TabInterface* tab);

  // Returns the formatted opener title for menu display. Returns empty string
  // if the controller doesn't exist or there's no valid opener.
  static std::u16string GetFormattedOpenerTitle(
      content::WebContents* web_contents);

  // Returns the opener favicon for menu display. Returns empty ImageModel if
  // the controller doesn't exist or there's no valid opener.
  static ui::ImageModel GetOpenerMenuIcon(content::WebContents* web_contents);

  // Set the opener WebContents and cache its information.
  void SetOpenerWebContents(content::WebContents* opener);

  // Returns true if this tab has a valid opener relationship. A tab has a valid
  // opener relationship when it has an opener tab (i.e. this tab was opened by
  // another tab) that has not been navigated from its original URL.
  bool HasValidOpener() const;

  // Returns true if the web contents has a valid opener relationship. Returns
  // false if the controller doesn't exist or the relationship is invalid.
  static bool HasValidOpener(content::WebContents* web_contents);

  // Returns true if back-to-opener navigation is available. This requires:
  // - A valid opener relationship exists (HasValidOpener() returns true)
  // - The destination tab is not pinned
  bool CanGoBackToOpener() const;

  // Returns true if back-to-opener navigation is available for the web
  // contents. Returns false if the controller doesn't exist or navigation is
  // not available.
  static bool CanGoBackToOpener(content::WebContents* web_contents);

  // Closes the current tab and activates the opener tab. This should only be
  // called when CanGoBackToOpener() returns true. The opener activation
  // happens after the tab is actually destroyed (after any unload prompts).
  void GoBackToOpener();

  // Closes the current tab and activates the opener tab if available. Does
  // nothing if the controller doesn't exist or navigation is not available.
  static void GoBackToOpener(content::WebContents* web_contents);

  // Called when the tab's pinned state changes. Updates internal state and
  // notifies UI that the back-to-opener availability may have changed.
  void OnPinnedStateChanged(tabs::TabInterface* tab, bool pinned);

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class OpenerWebContentsObserver;
  friend class TabCloseObserver;

  // Returns the original URL of the opener tab (the URL it had when the
  // relationship was established).
  GURL GetOpenerOriginalURL() const;

  // Notifies UI that the back-to-opener state has changed. Currently a no-op.
  // TODO(crbug.com/448173940): Update back button state (Back button
  // Interaction).
  void NotifyUIStateChanged();

  // Clears the opener relationship. Called when:
  // - The opener tab is destroyed
  // - The opener tab navigates away from its original URL
  void ClearOpenerRelationship();

  // Subscription management.
  base::CallbackListSubscription pinned_state_changed_subscription_;

  bool is_pinned_ = false;
  bool has_valid_opener_ = false;
  GURL opener_original_url_;
  std::u16string opener_title_;

  base::WeakPtr<content::WebContents> opener_web_contents_;

  std::unique_ptr<OpenerWebContentsObserver> opener_observer_;

  ui::ScopedUnownedUserData<BackToOpenerController> scoped_unowned_user_data_;

  base::WeakPtrFactory<BackToOpenerController> weak_factory_{this};
};

}  // namespace back_to_opener

#endif  // CHROME_BROWSER_UI_TABS_BACK_TO_OPENER_BACK_TO_OPENER_CONTROLLER_H_
