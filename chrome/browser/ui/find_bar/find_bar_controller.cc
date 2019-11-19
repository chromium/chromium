// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_controller.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

using content::NavigationController;
using content::WebContents;

namespace {

// The minimum space between the FindInPage window and the search result.
constexpr int kMinFindWndDistanceFromSelection = 5;

// Tracks windows and makes sure that closing the last Guest browser window
// clears the find pre-populate text.
class FindBrowserListObserver : public BrowserListObserver {
 public:
  FindBrowserListObserver() {
    BrowserList::AddObserver(this);
  }

  ~FindBrowserListObserver() override { BrowserList::RemoveObserver(this); }

  static void EnsureInstance() {
    static base::NoDestructor<FindBrowserListObserver> the_instance;
    the_instance.get();
  }

 protected:
  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    Profile* const guest_profile = GetGuestProfile(browser);
    if (!guest_profile)
      return;

    if (IsGuestWindowOpen())
      return;

    // Remove persistent find text across guest sessions. If we don't do this, a
    // future guest session in this browser process might get its find text
    // prepopulated with something that was searched in this session, which is a
    // violation of privacy expectations.
    FindBarState* const find_bar_state =
        FindBarStateFactory::GetForProfile(guest_profile);
    find_bar_state->set_last_prepopulate_text(base::string16());
  }

 private:
  // Returns a guest profile if the current browser has one, or nullptr
  // otherwise.
  static Profile* GetGuestProfile(Browser* browser) {
    Profile* profile = browser->profile();
    DCHECK(profile);
    return profile->IsGuestSession() ? profile : nullptr;
  }

  static bool IsGuestWindowOpen() {
    for (Browser* other : *BrowserList::GetInstance()) {
      if (GetGuestProfile(other))
        return true;
    }
    return false;
  }
};

}  // namespace

FindBarController::FindBarController(std::unique_ptr<FindBar> find_bar,
                                     Browser* browser)
    : find_bar_(std::move(find_bar)),
      browser_(browser),
      find_bar_platform_helper_(FindBarPlatformHelper::Create(this)) {
  FindBrowserListObserver::EnsureInstance();
}

FindBarController::~FindBarController() {
  DCHECK(!web_contents_);
}

void FindBarController::Show(bool find_next, bool forward_direction) {
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents_);

  // Only show the animation if we're not already showing a find bar for the
  // selected WebContents.
  if (!find_tab_helper->find_ui_active()) {
    has_user_modified_text_ = false;
    MaybeSetPrepopulateText();

    find_tab_helper->set_find_ui_active(true);
    find_bar_->Show(true);
  }
  find_bar_->SetFocusAndSelection();

  base::string16 find_text;
  if (!find_next && !has_user_modified_text_) {
    base::string16 selected_text = GetSelectedText();
    if (selected_text.length() <= 250)
      find_text = selected_text;
  }

#if defined(OS_MACOSX)
  // We always want to search for the current contents of the find bar on
  // OS X. For regular profile it's always the current find pboard. For
  // Incognito window it's the newest value of the find pboard content and
  // user-typed text.
  find_text = find_bar_->GetFindText();
#endif

  if (!find_text.empty() || find_next) {
    // Don't update the local input if we're using the global pasteboard.
    if (!find_bar_->HasGlobalFindPasteboard())
      find_bar_->SetFindTextAndSelectedRange(find_text,
                                             gfx::Range(0, find_text.length()));
    find_tab_helper->StartFinding(find_text, forward_direction, false);
  }
}

void FindBarController::EndFindSession(
    FindOnPageSelectionAction selection_action,
    FindBoxResultAction result_action) {
  find_bar_->Hide(true);

  // |web_contents_| can be NULL for a number of reasons, for example when the
  // tab is closing. We must guard against that case. See issue 8030.
  if (web_contents_) {
    FindTabHelper* find_tab_helper =
        FindTabHelper::FromWebContents(web_contents_);

    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tickmarks and highlighting.
    find_tab_helper->StopFinding(selection_action);

    if (result_action == FindBoxResultAction::kClear)
      find_bar_->ClearResults(find_tab_helper->find_result());

    // When we get dismissed we restore the focus to where it belongs.
    find_bar_->RestoreSavedFocus();
  }
}

void FindBarController::FindBarVisibilityChanged() {
  browser_->OnFindBarVisibilityChanged();
}

void FindBarController::ChangeWebContents(WebContents* contents) {
  if (web_contents_) {
    registrar_.RemoveAll();
    find_bar_->StopAnimation();

    FindTabHelper* find_tab_helper =
        FindTabHelper::FromWebContents(web_contents_);
    if (find_tab_helper) {
      find_tab_helper->set_selected_range(find_bar_->GetSelectedRange());
      find_tab_observer_.Remove(find_tab_helper);
    }
  }

  web_contents_ = contents;
  FindTabHelper* find_tab_helper =
      web_contents_ ? FindTabHelper::FromWebContents(web_contents_) : nullptr;
  if (find_tab_helper)
    find_tab_observer_.Add(find_tab_helper);

  // Hide any visible find window from the previous tab if a NULL tab contents
  // is passed in or if the find UI is not active in the new tab.
  if (find_bar_->IsFindBarVisible() &&
      (!find_tab_helper || !find_tab_helper->find_ui_active())) {
    find_bar_->Hide(false);
  }

  if (!web_contents_)
    return;

  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&web_contents_->GetController()));

  MaybeSetPrepopulateText();

  if (find_tab_helper && find_tab_helper->find_ui_active()) {
    // A tab with a visible find bar just got selected and we need to show the
    // find bar but without animation since it was already animated into its
    // visible state. We also want to reset the window location so that
    // we don't surprise the user by popping up to the left for no apparent
    // reason.
    find_bar_->Show(false);
  }

  UpdateFindBarForCurrentResult();
  find_bar_->UpdateFindBarForChangedWebContents();
}

void FindBarController::SetText(base::string16 text) {
  find_bar_->SetFindTextAndSelectedRange(text, find_bar_->GetSelectedRange());
}

void FindBarController::OnUserChangedFindText(base::string16 text) {
  has_user_modified_text_ = !text.empty();

  if (find_bar_platform_helper_)
    find_bar_platform_helper_->OnUserChangedFindText(text);
}

////////////////////////////////////////////////////////////////////////////////
// FindBarController, content::NotificationObserver implementation:

void FindBarController::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  NavigationController* source_controller =
      content::Source<NavigationController>(source).ptr();
  if (source_controller == &web_contents_->GetController()) {
    content::LoadCommittedDetails* commit_details =
        content::Details<content::LoadCommittedDetails>(details).ptr();
    ui::PageTransition transition_type =
        commit_details->entry->GetTransitionType();
    // Hide the find bar on reload or navigation.
    if (find_bar_->IsFindBarVisible() && commit_details->is_main_frame &&
        (ui::PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_RELOAD) ||
         commit_details->is_navigation_to_different_page()))
      EndFindSession(FindOnPageSelectionAction::kKeep,
                     FindBoxResultAction::kClear);
  }
}

// static
gfx::Rect FindBarController::GetLocationForFindbarView(
    gfx::Rect view_location,
    const gfx::Rect& dialog_bounds,
    const gfx::Rect& avoid_overlapping_rect) {
  if (base::i18n::IsRTL()) {
    int boundary = dialog_bounds.width() - view_location.width();
    view_location.set_x(std::min(view_location.x(), boundary));
  } else {
    view_location.set_x(std::max(view_location.x(), dialog_bounds.x()));
  }

  gfx::Rect new_pos = view_location;

  // If the selection rectangle intersects the current position on screen then
  // we try to move our dialog to the left (right for RTL) of the selection
  // rectangle.
  if (!avoid_overlapping_rect.IsEmpty() &&
      avoid_overlapping_rect.Intersects(new_pos)) {
    if (base::i18n::IsRTL()) {
      new_pos.set_x(avoid_overlapping_rect.x() +
                    avoid_overlapping_rect.width() +
                    (2 * kMinFindWndDistanceFromSelection));

      // If we moved it off-screen to the right, we won't move it at all.
      if (new_pos.x() + new_pos.width() > dialog_bounds.width())
        new_pos = view_location;  // Reset.
    } else {
      new_pos.set_x(avoid_overlapping_rect.x() - new_pos.width() -
        kMinFindWndDistanceFromSelection);

      // If we moved it off-screen to the left, we won't move it at all.
      if (new_pos.x() < 0)
        new_pos = view_location;  // Reset.
    }
  }

  return new_pos;
}

void FindBarController::OnFindResultAvailable(
    content::WebContents* web_contents) {
  DCHECK_EQ(web_contents, web_contents_);
  UpdateFindBarForCurrentResult();

  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents_);

  // Only "final" results may audibly alert the user.
  if (!find_tab_helper->find_result().final_update())
    return;

  const base::string16& current_search = find_tab_helper->find_text();

  // If no results were found, play an audible alert (depending upon platform
  // convention). Alert only once per unique search, and don't alert on
  // backspace.
  if ((find_tab_helper->find_result().number_of_matches() == 0) &&
      (current_search != find_tab_helper->last_completed_find_text() &&
       !base::StartsWith(find_tab_helper->previous_find_text(), current_search,
                         base::CompareCase::SENSITIVE))) {
    find_bar_->AudibleAlert();
  }

  // Record the completion of the search to suppress future alerts, even if the
  // page's contents change.
  find_tab_helper->set_last_completed_find_text(current_search);
}

void FindBarController::UpdateFindBarForCurrentResult() {
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents_);
  const FindNotificationDetails& find_result = find_tab_helper->find_result();
  // Avoid bug 894389: When a new search starts (and finds something) it reports
  // an interim match count result of 1 before the scoping effort starts. This
  // is to provide feedback as early as possible that we will find something.
  // As you add letters to the search term, this creates a flashing effect when
  // we briefly show "1 of 1" matches because there is a slight delay until
  // the scoping effort starts updating the match count. We avoid this flash by
  // ignoring interim results of 1 if we already have a positive number.
  if (find_result.number_of_matches() > -1) {
    if (last_reported_matchcount_ > 0 && find_result.number_of_matches() == 1 &&
        !find_result.final_update() &&
        last_reported_ordinal_ == find_result.active_match_ordinal()) {
      return;  // Don't let interim result override match count.
    }
    last_reported_matchcount_ = find_result.number_of_matches();
    last_reported_ordinal_ = find_result.active_match_ordinal();
  }

  find_bar_->UpdateUIForFindResult(find_result, find_tab_helper->find_text());
}

void FindBarController::MaybeSetPrepopulateText() {
  // Having a per-tab find_string is not compatible with a global find
  // pasteboard, so we always have the same find text in all find bars. This is
  // done through the find pasteboard mechanism, so don't set the text here.
  if (find_bar_->HasGlobalFindPasteboard())
    return;

  // Find out what we should show in the find text box. Usually, this will be
  // the last search in this tab, but if no search has been issued in this tab
  // we use the last search string (from any tab).
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents_);
  base::string16 find_string = find_tab_helper->find_text();
  if (find_string.empty())
    find_string = find_tab_helper->previous_find_text();
  if (find_string.empty()) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    find_string = FindBarStateFactory::GetLastPrepopulateText(profile);
  }

  // Update the find bar with existing results and search text, regardless of
  // whether or not the find bar is visible, so that if it's subsequently
  // shown it is showing the right state for this tab. We update the find text
  // _first_ since the FindBarView checks its emptiness to see if it should
  // clear the result count display when there's nothing in the box.
  find_bar_->SetFindTextAndSelectedRange(find_string,
                                         find_tab_helper->selected_range());
}

base::string16 FindBarController::GetSelectedText() {
  auto* host_view = web_contents_->GetRenderWidgetHostView();
  if (!host_view)
    return base::string16();

  base::string16 selected_text = host_view->GetSelectedText();
  // This should be kept in sync with what TextfieldModel::Paste() does, since
  // that's what would run if the user explicitly pasted this text into the find
  // bar.
  base::TrimWhitespace(selected_text, base::TRIM_ALL, &selected_text);
  return selected_text;
}
