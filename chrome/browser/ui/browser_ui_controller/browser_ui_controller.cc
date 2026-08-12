// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_controller/browser_ui_controller.h"

#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"

namespace {

// The time in milliseconds that we coalesce UI updates.
constexpr base::TimeDelta kUIUpdateCoalescingTime = base::Milliseconds(200);

}  // namespace

DEFINE_USER_DATA(BrowserUiController);

BrowserUiController::BrowserUiController(
    BrowserWindowInterface& browser,
    TabStripModel& tab_strip_model,
    BrowserWindow& window,
    BookmarkBarController& bookmark_bar_controller)
    : browser_(browser),
      tab_strip_model_(tab_strip_model),
      window_(window),
      bookmark_bar_controller_(bookmark_bar_controller),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {}

BrowserUiController::~BrowserUiController() = default;

// static
BrowserUiController* BrowserUiController::From(
    BrowserWindowInterface* browser) {
  CHECK(browser);
  return ui::ScopedUnownedUserData<BrowserUiController>::Get(
      browser->GetUnownedUserDataHost());
}

// static
const BrowserUiController* BrowserUiController::From(
    const BrowserWindowInterface* browser) {
  CHECK(browser);
  return ui::ScopedUnownedUserData<BrowserUiController>::Get(
      browser->GetUnownedUserDataHost());
}

void BrowserUiController::UpdateUIForNavigationInTab(
    content::WebContents* contents,
    ui::PageTransition transition,
    NavigateParams::WindowAction action,
    bool user_initiated) {
  tab_strip_model_->TabNavigating(contents, transition);

  bool contents_is_selected =
      contents == tab_strip_model_->GetActiveWebContents();
  if (user_initiated && contents_is_selected && window_->GetLocationBar()) {
    // Forcibly reset the location bar if the url is going to change in the
    // current tab, since otherwise it won't discard any ongoing user edits,
    // since it doesn't realize this is a user-initiated action.
    window_->GetLocationBar()->Revert();
  }

  std::vector<StatusBubble*> status_bubbles = GetStatusBubbles();
  for (StatusBubble* status_bubble : status_bubbles) {
    status_bubble->Hide();
  }

  // Update the location bar. This is synchronous. We specifically don't
  // update the load state since the load hasn't started yet and updating it
  // will put it out of sync with the actual state like whether we're
  // displaying a favicon, which controls the throbber. If we updated it here,
  // the throbber will show the default favicon for a split second when
  // navigating away from the new tab page.
  ScheduleUIUpdate(contents, content::INVALIDATE_TYPE_URL);

  // Navigating contents can take focus (potentially taking it away from other,
  // currently-focused UI element like the omnibox) if the navigation was
  // initiated by the user (e.g., via omnibox, bookmarks, etc.).
  //
  // Note that focusing contents of NTP-initiated navigations is taken care of
  // elsewhere - see FocusTabAfterNavigationHelper.
  if (user_initiated && contents_is_selected &&
      (window_->IsActive() ||
       action == NavigateParams::WindowAction::kShowWindow)) {
    contents->SetInitialFocus();
  }
}

void BrowserUiController::ScheduleUIUpdate(content::WebContents* source,
                                           unsigned changed_flags) {
  DCHECK(source);
  // WebContents may in some rare cases send updates after they've been detached
  // from the tabstrip but before they are deleted, causing a potential crash if
  // we proceed. For now bail out.
  // TODO(crbug.com/40100269) Figure out a safe way to detach browser delegate
  // from WebContents when it's removed so this doesn't happen - then put a
  // DCHECK back here.
  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(source);
  if (!tab || tab->GetBrowserWindowInterface() != &*browser_) {
    return;
  }

  // Do some synchronous updates.
  if (changed_flags & content::INVALIDATE_TYPE_URL) {
    if (source == tab_strip_model_->GetActiveWebContents()) {
      // Only update the URL for the current tab. Note that we do not update
      // the navigation commands since those would have already been updated
      // synchronously by NavigationStateChanged.
      UpdateToolbar(false);
    } else {
      // Clear the saved tab state for the tab that navigated, so that we don't
      // restore any user text after the old URL has been invalidated (e.g.,
      // after a new navigation commits in that tab while unfocused).
      window_->ResetToolbarTabState(source);
    }
    changed_flags &= ~content::INVALIDATE_TYPE_URL;
  }

  if (changed_flags & content::INVALIDATE_TYPE_LOAD) {
    // Update the loading state synchronously. This is so the throbber will
    // immediately start/stop, which gives a more snappy feel. We want to do
    // this for any tab so they start & stop quickly.
    NotifyTabUIChanged(tab, TabChangeType::kLoadingOnly);
    // The status bubble needs to be updated during INVALIDATE_TYPE_LOAD too,
    // but we do that asynchronously by not stripping INVALIDATE_TYPE_LOAD from
    // changed_flags.
  }

  // If the only updates were synchronously handled above, we're done.
  if (changed_flags == 0) {
    return;
  }

  // Save the dirty bits.
  scheduled_updates_[tab] |= changed_flags;

  if (!chrome_updater_factory_.HasWeakPtrs()) {
    base::TimeDelta delay = update_ui_immediately_for_testing_
                                ? base::Milliseconds(0)
                                : kUIUpdateCoalescingTime;
    // No task currently scheduled, start another.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BrowserUiController::ProcessPendingUIUpdates,
                       chrome_updater_factory_.GetWeakPtr()),
        delay);
  }
}

void BrowserUiController::ProcessPendingUIUpdates() {
#ifndef NDEBUG
  // Validate that all tabs we have pending updates for exist. This is scary
  // because the pending list must be kept in sync with any detached or
  // deleted tabs.
  size_t processed_count = 0;
  for (tabs::TabInterface* tab : *tab_strip_model_) {
    if (scheduled_updates_.find(tab) != scheduled_updates_.end()) {
      processed_count++;
    }
  }
  DCHECK_EQ(processed_count, scheduled_updates_.size());
#endif

  chrome_updater_factory_.InvalidateWeakPtrs();

  for (const auto& [tab, flags] : scheduled_updates_) {
    if (tab->IsActivated()) {
      // Updates that only matter when the tab is selected go here.

      // Updating the URL happens synchronously in ScheduleUIUpdate.
      std::vector<StatusBubble*> status_bubbles = GetStatusBubbles();
      if (flags & content::INVALIDATE_TYPE_LOAD && !status_bubbles.empty()) {
        status_bubbles.front()->SetStatus(
            CoreTabHelper::FromWebContents(tab->GetContents())
                ->GetStatusText());
      }

      if (flags &
          (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
        window_->UpdateTitleBar();
      }
    }

    // Updates that don't depend upon the selected state go here.
    if (flags & (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE |
                 content::INVALIDATE_TYPE_AUDIO)) {
      NotifyTabUIChanged(tab, TabChangeType::kAll);
    }

    // Update the bookmark bar and PWA install icon. It may happen that the tab
    // is crashed, and if so, the bookmark bar and PWA install icon should be
    // hidden.
    if (flags & content::INVALIDATE_TYPE_TAB) {
      // Update bookmark bar state with kTabState to handle tab state changes
      // (like crashes). This is different from kTabSwitch which is already
      // handled in Browser::OnActiveTabChanged().
      bookmark_bar_controller_->UpdateBookmarkBarState(
          BookmarkBarController::StateChangeReason::kTabState);
    }

    // We don't need to process INVALIDATE_STATE, since that's not visible.
  }

  scheduled_updates_.clear();
}

void BrowserUiController::RemoveScheduledUpdatesFor(
    content::WebContents* contents) {
  if (!contents) {
    return;
  }

  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(contents);
  if (tab) {
    scheduled_updates_.erase(tab);
  }
}

void BrowserUiController::UpdateToolbar(bool should_restore_state) {
  TRACE_EVENT0("ui", "BrowserUiController::UpdateToolbar");
  window_->UpdateToolbar(should_restore_state
                             ? tab_strip_model_->GetActiveWebContents()
                             : nullptr);
}

void BrowserUiController::UpdateToolbarSecurityState() {
  TRACE_EVENT0("ui", "BrowserUiController::UpdateToolbarSecurityState");
  window_->UpdateToolbarSecurityState();
}

void BrowserUiController::NotifyTabUIChanged(tabs::TabInterface* tab,
                                             TabChangeType change_type) {
  tab_strip_model_->NotifyTabChanged(tab, change_type);
  TabUIHelper::From(tab)->NotifyTabUIChanged(
      base::PassKey<BrowserUiController>());
}

std::vector<StatusBubble*> BrowserUiController::GetStatusBubbles() {
  // For kiosk and exclusive app mode we want to always hide the status bubble.
  if (IsRunningInAppMode()) {
    return {};
  }

  // We hide the status bar for web apps windows as this matches native
  // experience. However, we include the status bar for 'minimal-ui' display
  // mode, as the minimal browser UI includes the status bar.
  auto* const app_browser_controller =
      web_app::AppBrowserController::From(&*browser_);
  if (app_browser_controller &&
      !app_browser_controller->HasMinimalUiButtons()) {
    return {};
  }

  return window_->GetStatusBubbles();
}

void BrowserUiController::UpdateWindowForLoadingStateChanged(
    content::WebContents* source,
    bool should_show_loading_ui) {
  window_->UpdateLoadingAnimations(/*is_visible=*/!window_->IsMinimized());
  window_->UpdateTitleBar();

  content::WebContents* selected_contents =
      tab_strip_model_->GetActiveWebContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading() && should_show_loading_ui;
    chrome::BrowserCommandController::From(&*browser_)
        ->LoadingStateChanged(is_loading, false);

    std::vector<StatusBubble*> status_bubbles = window_->GetStatusBubbles();
    if (!status_bubbles.empty()) {
      status_bubbles.front()->SetStatus(
          CoreTabHelper::FromWebContents(selected_contents)->GetStatusText());
    }
  }
}
