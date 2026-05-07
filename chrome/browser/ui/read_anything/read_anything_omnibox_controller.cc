// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_omnibox_controller.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "read_anything_entry_point_controller.h"
#include "ui/accessibility/accessibility_features.h"

ReadAnythingOmniboxController::ReadAnythingOmniboxController(
    tabs::TabInterface* tab)
    : tabs::ContentsObservingTabFeature(*tab),
      PageActionObserver(kActionSidePanelShowReadAnything),
      tab_(tab) {
  // This class should only be instantiated if the omnibox entrypoint is
  // enabled.
  CHECK(features::IsReadAnythingOmniboxChipEnabled() &&
        base::FeatureList::IsEnabled(features::kPageActionsMigration));

  read_anything::ReadAnythingEntryPointController::
      RegisterForSuggestReadingMode(
          tab_->GetBrowserWindowInterface()->GetProfile());

  RegisterAsPageActionObserver(
      *tab_->GetTabFeatures()->page_action_controller());
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(
      base::BindRepeating(&ReadAnythingOmniboxController::TabWillDetach,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(
      base::BindRepeating(&ReadAnythingOmniboxController::OnTabForegrounded,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&ReadAnythingOmniboxController::OnTabBackgrounded,
                          weak_factory_.GetWeakPtr())));

  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* read_anything_controller = ReadAnythingController::From(tab_);
    CHECK(read_anything_controller);
    read_anything_controller->AddObserver(this);
  }
}

ReadAnythingOmniboxController::~ReadAnythingOmniboxController() = default;

void ReadAnythingOmniboxController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // Use the cached was_last_checked_page_distillable_ since
  // GetCurrentPageAction().showing will already be false if it was showing
  // before. If it was a good candidate, then it's likely the entry point was
  // showing, so mark it as "ignored".
  UpdateIgnored(was_last_checked_page_distillable_);
}

void ReadAnythingOmniboxController::OnTabForegrounded(tabs::TabInterface* tab) {
  if (!was_page_checked_) {
    DebounceCheckSuggestion();
  }
}

void ReadAnythingOmniboxController::OnTabBackgrounded(tabs::TabInterface* tab) {
  StopTimers();
}

void ReadAnythingOmniboxController::Activate(
    bool active,
    std::optional<ReadAnythingOpenTrigger> open_trigger,
    std::optional<base::TimeDelta> completed_session_duration) {
  if (active) {
    if (iph_response_timer_ && iph_response_timer_->IsRunning()) {
      iph_response_timer_->Stop();
      RecordOpenedAfterPromo();
    }

    if (features::IsReadAnythingOmniboxChipEnabled() &&
        base::FeatureList::IsEnabled(features::kPageActionsMigration) &&
        open_trigger.has_value() &&
        open_trigger.value() !=
            ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton &&
        GetCurrentPageActionState().showing) {
      // Ignore the toggle presentation button for this metric, since that can
      // only be used after RM is already open.
      base::UmaHistogramEnumeration(
          "Accessibility.ReadAnything.EntryPointAfterOmnibox",
          open_trigger.value());
    }
    // Hide the omnibox entrypoint now that RM is already showing.
    read_anything::ReadAnythingEntryPointController::UpdatePageActionVisibility(
        /*should_show_page_action=*/false, tab_);
  } else if (!features::IsImmersiveReadAnythingEnabled() &&
             tab_->IsActivated()) {
    // Show the entrypoint again once RM is closed. In immersive mode, do this
    // in OnReadingModePresenterChanged instead since the presentation state
    // does not change right away.
    read_anything::ReadAnythingEntryPointController::UpdatePageActionVisibility(
        /*should_show_page_action=*/true, tab_);
  }
}

void ReadAnythingOmniboxController::OnReadingModePresenterChanged() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }

  auto* read_anything_controller = ReadAnythingController::From(tab_);
  CHECK(read_anything_controller);
  // If Reading mode was just closed by the user, show the omnibox entrypoint.
  if (read_anything_controller->GetPresentationState() ==
          ReadAnythingController::PresentationState::kInactive &&
      last_close_reason_.has_value() &&
      last_close_reason_ == ReadAnythingCloseReason::kClosedByUser) {
    read_anything::ReadAnythingEntryPointController::UpdatePageActionVisibility(
        /*should_show_page_action=*/true, tab_);
  }
}

void ReadAnythingOmniboxController::OnDestroyed() {
  StopTimers();
  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* read_anything_controller = ReadAnythingController::From(tab_);
    CHECK(read_anything_controller);
    read_anything_controller->RemoveObserver(this);
  }
}

void ReadAnythingOmniboxController::OnWillClose(
    ReadAnythingCloseReason reason) {
  CHECK(features::IsImmersiveReadAnythingEnabled());
  last_close_reason_ = reason;
}

void ReadAnythingOmniboxController::OnDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  tabs::ContentsObservingTabFeature::OnDiscardContents(tab, old_contents,
                                                       new_contents);
  read_anything::ReadAnythingEntryPointController::UpdatePageActionVisibility(
      /*should_show_page_action=*/false, tab_);
  StopTimers();
  was_last_checked_page_distillable_ = false;
  was_page_checked_ = false;
  candidate_check_triggered_time_ms_ = base::TimeTicks();
}

void ReadAnythingOmniboxController::PrimaryPageChanged(content::Page& page) {
  // Reset the distillable indicator when the page changes.
  was_last_checked_page_distillable_ = false;
  was_page_checked_ = false;
  if (IsIrrelevant()) {
    return;
  }

  StopTimers();
  UpdateIgnored(GetCurrentPageActionState().showing);
  DebounceCheckSuggestion();
}

void ReadAnythingOmniboxController::DidStopLoading() {
  if (check_suggestion_debouncer_ && check_suggestion_debouncer_->IsRunning()) {
    check_suggestion_debouncer_->Reset();
  }
}

void ReadAnythingOmniboxController::DebounceCheckSuggestion() {
  if (IsIrrelevant()) {
    return;
  }

  candidate_check_triggered_time_ms_ = base::TimeTicks::Now();
  if (!check_suggestion_debouncer_) {
    check_suggestion_debouncer_ = std::make_unique<base::OneShotTimer>();
  }
  check_suggestion_debouncer_->Start(
      FROM_HERE, base::Seconds(kDebounceDelaySecs),
      base::BindOnce(
          &ReadAnythingOmniboxController::CheckIfShouldSuggestReadingMode,
          weak_factory_.GetWeakPtr()));
}

void ReadAnythingOmniboxController::CheckIfShouldSuggestReadingMode() {
  if (IsIrrelevant()) {
    return;
  }

  read_anything::ReadAnythingEntryPointController::
      CheckIfShouldSuggestReadingMode(
          tab_->GetBrowserWindowInterface(),
          base::BindOnce(
              &ReadAnythingOmniboxController::OnShouldSuggestReadingModeResult,
              weak_factory_.GetWeakPtr()));
}

void ReadAnythingOmniboxController::OnShouldSuggestReadingModeResult(
    bool should_show) {
  // Cache the result of CheckIfShouldSuggestReadingMode since the omnibox
  // entry point is hidden when the tab is closed, but a closed tab should
  // count as "ignored".
  was_last_checked_page_distillable_ = should_show;
  was_page_checked_ = true;
  if (IsIrrelevant()) {
    return;
  }

  UpdateVisibility(should_show);
}

void ReadAnythingOmniboxController::UpdateVisibility(bool should_show) {
  // Don't show the entrypoint if the tab is no longer active.
  if (IsIrrelevant()) {
    return;
  }

  read_anything::ReadAnythingEntryPointController::UpdatePageActionVisibility(
      should_show, tab_,
      base::BindOnce(&ReadAnythingOmniboxController::OnShowPromoResult,
                     weak_factory_.GetWeakPtr()));
}

void ReadAnythingOmniboxController::UpdateIgnored(bool is_showing) {
  if (!is_showing) {
    return;
  }

  // Indicate that the omnibox entrypoint was ignored if it's still showing when
  // the page changes or tab closes, and the user was on the previous page for a
  // non-trivial amount of time. Without this time check, the omnibox would be
  // snoozed if the user is quickly clicking through links without reading them,
  // so they aren't truly ignoring the Reading mode entrypoint.
  base::TimeDelta time_on_previous_page =
      candidate_check_triggered_time_ms_.is_null()
          ? base::Milliseconds(0)
          : base::TimeTicks::Now() - candidate_check_triggered_time_ms_;
  if (time_on_previous_page.InMilliseconds() >
      kTimeOnPreviousPageBeforeIgnored) {
    read_anything::ReadAnythingEntryPointController::OnPageActionIgnored(
        tab_->GetBrowserWindowInterface());
  }
}

void ReadAnythingOmniboxController::OnShowPromoResult(
    user_education::FeaturePromoResult result) {
  if (result == user_education::FeaturePromoResult::Success()) {
    iph_response_timer_ = std::make_unique<base::OneShotTimer>();
    iph_response_timer_->Start(
        FROM_HERE, base::Seconds(kIPHResponseTimeoutSecs),
        base::BindOnce(&ReadAnythingOmniboxController::RecordOpenedAfterPromo,
                       base::Unretained(this)));
  }
}

void ReadAnythingOmniboxController::RecordOpenedAfterPromo() {
  base::UmaHistogramBoolean(
      "Accessibility.ReadAnything.OpenedAfterOmniboxIPH",
      read_anything::ReadAnythingEntryPointController::IsUIShowing(
          tab_->GetBrowserWindowInterface()));
}

void ReadAnythingOmniboxController::StopTimers() {
  if (iph_response_timer_ && iph_response_timer_->IsRunning()) {
    iph_response_timer_->Stop();
    // If the IPH response timer is running and is stopped early, record whether
    // the user opened RM after seeing the IPH.
    RecordOpenedAfterPromo();
  }
  if (check_suggestion_debouncer_ && check_suggestion_debouncer_->IsRunning()) {
    check_suggestion_debouncer_->Stop();
  }
}

bool ReadAnythingOmniboxController::IsIrrelevant() {
  return !tab_->IsActivated() ||
         read_anything::ReadAnythingEntryPointController::IsUIShowing(
             tab_->GetBrowserWindowInterface());
}
