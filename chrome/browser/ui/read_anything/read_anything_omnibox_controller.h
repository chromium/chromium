// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_OMNIBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_OMNIBOX_CONTROLLER_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/web_contents_observer.h"

// A per-tab class that handles the logic for showing or hiding the omnibox
// entry point for Reading mode.
class ReadAnythingOmniboxController : public content::WebContentsObserver,
                                      public page_actions::PageActionObserver,
                                      public ReadAnythingLifecycleObserver {
 public:
  explicit ReadAnythingOmniboxController(tabs::TabInterface* tab);

  ReadAnythingOmniboxController(const ReadAnythingOmniboxController&) = delete;
  ReadAnythingOmniboxController& operator=(
      const ReadAnythingOmniboxController&) = delete;
  ~ReadAnythingOmniboxController() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidStopLoading() override;

  // ReadAnythingLifecycleObserver:
  void Activate(bool active,
                std::optional<ReadAnythingOpenTrigger> open_trigger) override;
  void OnDestroyed() override;
  void OnReadingModePresenterChanged() override;

  void SetDwellTimeForTesting(base::TimeTicks test_time) {
    candidate_check_triggered_time_ms_ = test_time;
  }

 protected:
  // Runs a heuristic to check if the current tab's contents are a good
  // candidate for distillation in Reading mode. The result is returned in the
  // OnReadabilityResult call below, and is used to determine whether or not to
  // show the omnibox entrypoint for RM.
  virtual void CheckIfShouldSuggestReadingMode();

 private:
  // The amount of time the user must spend on the previous page before it seems
  // they are ignoring the omnibox entry point.
  static const int kTimeOnPreviousPageBeforeIgnored = 3000;
  // Delay before logging whether the user opened RM after seeing the IPH for
  // the omnibox entrypoint. If they don't open RM within this time, log that
  // they didn't open it, as it's unlikely the IPH convinced them to open RM.
  static const int kIPHResponseTimeoutSecs = 20;
  // Delay before checking again if to suggest reading mode. Running the check
  // can be CPU-intensive, so don't overload it.
  static const int kDebounceDelaySecs = 1;

  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);
  void OnTabBackgrounded(tabs::TabInterface* tab);
  void OnTabForegrounded(tabs::TabInterface* tab);

  // Runs CheckIfShouldSuggestReadingMode after a delay to debounce multiple
  // calls to it during page or tab load.
  void DebounceCheckSuggestion();

  // Called with the results of CheckIfShouldSuggestReadingMode.
  void OnShouldSuggestReadingModeResult(bool should_show);

  // Show or hide the omnibox entry point.
  void UpdateVisibility(bool should_show);

  // Checks if the omnibox entry point was ignored and informs the entry point
  // controller if it was.
  void UpdateIgnored(bool is_showing);

  // Called when the IPH for the omnibox entry is either shown or not shown.
  void OnShowPromoResult(user_education::FeaturePromoResult result);

  // Log whether the user opened RM after seeing the omnibox IPH.
  void RecordOpenedAfterPromo();

  // Stops any running timers.
  void StopTimers();

  // The time when CheckIfShouldSuggestReadingMode was triggered.
  base::TimeTicks candidate_check_triggered_time_ms_;

  // The cached result of CheckIfShouldSuggestReadingMode.
  bool was_last_checked_page_distillable_ = false;

  // A timer for logging whether the user opened RM after seeing the IPH.
  std::unique_ptr<base::OneShotTimer> iph_response_timer_;
  std::unique_ptr<base::OneShotTimer> check_suggestion_debouncer_;

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  base::WeakPtrFactory<ReadAnythingOmniboxController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_OMNIBOX_CONTROLLER_H_
