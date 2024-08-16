// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_controller.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_platform_helper.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"

using content::NavigationController;
using content::WebContents;

FindBarController::FindBarController(std::unique_ptr<FindBar> find_bar)
    : find_bar_(std::move(find_bar)),
      find_bar_platform_helper_(FindBarPlatformHelper::Create(this)) {}

FindBarController::~FindBarController() {
  DCHECK(!web_contents());
}

void FindBarController::Show(bool find_next, bool forward_direction) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  // Only show the animation if we're not already showing a find bar for the
  // selected WebContents.
  if (!find_tab_helper->find_ui_active()) {
    has_user_modified_text_ = false;
    MaybeSetPrepopulateText();

    find_tab_helper->set_find_ui_active(true);
    find_bar_->Show(true);
  }
  find_bar_->SetFocusAndSelection();

  if (find_next) {
    find_tab_helper->StartFinding(find_bar_->GetFindText(), forward_direction,
                                  false /* case_sensitive */,
                                  true /* find_match */);
    return;
  }

  if (has_user_modified_text_)
    return;

  std::u16string selected_text = GetSelectedText();
  auto selected_length = selected_text.length();
  if (selected_length > 0 && selected_length <= 250) {
    find_bar_->SetFindTextAndSelectedRange(
        selected_text, gfx::Range(0, selected_text.length()));
  }
  // Since this isn't a find-next operation, we don't want to jump to any
  // matches. Doing so could cause the page to scroll when a user is just
  // trying to pull up the find bar — they might not even want to search for
  // whatever is prefilled (e.g. the selected text or the global pasteboard).
  // So we set |find_match| to false, which will set up match counts and
  // highlighting, but not jump to any matches.
  find_tab_helper->StartFinding(find_bar_->GetFindText(),
                                true /* forward_direction */,
                                false /* case_sensitive */,
                                false /* find_match */);
}

void FindBarController::EndFindSession(
    find_in_page::SelectionAction selection_action,
    find_in_page::ResultAction result_action) {
  find_bar_->Hide(true);

  // web_contents() can be NULL for a number of reasons, for example when the
  // tab is closing. We must guard against that case. See issue 8030.
  if (web_contents()) {
    find_in_page::FindTabHelper* find_tab_helper =
        find_in_page::FindTabHelper::FromWebContents(web_contents());

    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tickmarks and highlighting.
    find_tab_helper->StopFinding(selection_action);

    if (result_action == find_in_page::ResultAction::kClear)
      find_bar_->ClearResults(find_tab_helper->find_result());

    // When we get dismissed we restore the focus to where it belongs.
    find_bar_->RestoreSavedFocus();
  }
}

void FindBarController::ChangeWebContents(WebContents* contents) {
  if (web_contents()) {
    find_bar_->StopAnimation();

    find_in_page::FindTabHelper* find_tab_helper =
        find_in_page::FindTabHelper::FromWebContents(web_contents());
    if (find_tab_helper) {
      find_tab_helper->set_selected_range(find_bar_->GetSelectedRange());
      DCHECK(find_tab_observation_.IsObservingSource(find_tab_helper));
      find_tab_observation_.Reset();
    }
  }

  find_in_page::FindTabHelper* find_tab_helper =
      contents ? find_in_page::FindTabHelper::FromWebContents(contents)
               : nullptr;
  if (find_tab_helper)
    find_tab_observation_.Observe(find_tab_helper);

  // Hide any visible find window from the previous tab if a NULL tab contents
  // is passed in or if the find UI is not active in the new tab.
  if (find_bar_->IsFindBarVisible() &&
      (!find_tab_helper || !find_tab_helper->find_ui_active())) {
    find_bar_->Hide(false);
  }

  Observe(contents);

  if (!web_contents()) {
    return;
  }

  MaybeSetPrepopulateText();
  UpdateFindBarForCurrentResult();

  if (find_tab_helper && find_tab_helper->find_ui_active()) {
    // A tab with a visible find bar just got selected and we need to show the
    // find bar but without animation since it was already animated into its
    // visible state. We also want to reset the window location so that
    // we don't surprise the user by popping up to the left for no apparent
    // reason.
    find_bar_->Show(false);
    // The condition below can be true on macOS if the global pasteboard changed
    // while this tab was inactive (the find result will have been reset by
    // FindBarPlatformHelperMac). In that case, we need to find the new text to
    // update the results in the findbar. If condition is true due to the find
    // text being empty, the call to StartFinding will be a harmless no-op.
    if (find_tab_helper->find_result().number_of_matches() == -1) {
      find_tab_helper->StartFinding(find_bar_->GetFindText(),
                                    true /* forward_direction */,
                                    false /* case_sensitive */,
                                    false /* find_match */);
    }
  }

  find_bar_->UpdateFindBarForChangedWebContents();
}

void FindBarController::SetText(std::u16string text) {
  find_bar_->SetFindTextAndSelectedRange(text, find_bar_->GetSelectedRange());

  if (!web_contents()) {
    return;
  }
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());
  if (!find_tab_helper->find_ui_active())
    return;

  find_tab_helper->StartFinding(text,
                                true /* forward_direction */,
                                false /* case_sensitive */,
                                false /* find_match */);
}

void FindBarController::OnUserChangedFindText(std::u16string text) {
  has_user_modified_text_ = !text.empty();

  if (find_bar_platform_helper_)
    find_bar_platform_helper_->OnUserChangedFindText(text);
}

////////////////////////////////////////////////////////////////////////////////
// FindBarController, content::WebContentsObserver implementation:

void FindBarController::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Hide the find bar on navigation.
  if (find_bar_->IsFindBarVisible() && load_details.is_main_frame &&
      load_details.is_navigation_to_different_page()) {
    EndFindSession(find_in_page::SelectionAction::kKeep,
                   find_in_page::ResultAction::kClear);
  }
}

void FindBarController::OnFindEmptyText(content::WebContents* web_contents) {
  CHECK_EQ(web_contents, this->web_contents());
  UpdateFindBarForCurrentResult();
}

void FindBarController::OnFindResultAvailable(
    content::WebContents* web_contents) {
  CHECK_EQ(web_contents, this->web_contents());
  UpdateFindBarForCurrentResult();

  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);

  // Only "final" results may audibly alert the user. Also don't alert when
  // we're only highlighting results (when first opening the find bar).
  // See https://crbug.com/1131780
  if (!find_tab_helper->find_result().final_update() ||
      !find_tab_helper->should_find_match())
    return;

  const std::u16string& current_search = find_tab_helper->find_text();

  // If no results were found, play an audible alert (depending upon platform
  // convention). Alert only once per unique search, and don't alert on
  // backspace.
  if ((find_tab_helper->find_result().number_of_matches() == 0) &&
      !base::StartsWith(find_tab_helper->last_completed_find_text(),
                        current_search, base::CompareCase::SENSITIVE)) {
    find_bar_->AudibleAlert();
  }

  // Record the completion of the search to suppress future alerts, even if the
  // page's contents change.
  find_tab_helper->set_last_completed_find_text(current_search);
}

void FindBarController::UpdateFindBarForCurrentResult() {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());
  const find_in_page::FindNotificationDetails& find_result =
      find_tab_helper->find_result();
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
  // done through the find pasteboard mechanism (see FindBarPlatformHelperMac),
  // so don't set the text here.
  if (find_bar_->HasGlobalFindPasteboard())
    return;

  // Find out what we should show in the find text box. Usually, this will be
  // the last search in this tab, but if no search has been issued in this tab
  // we use the last search string (from any tab).
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());
  std::u16string find_string = find_tab_helper->find_text();
  if (find_string.empty())
    find_string = find_tab_helper->GetInitialSearchText();

  // Update the find bar with existing results and search text, regardless of
  // whether or not the find bar is visible, so that if it's subsequently
  // shown it is showing the right state for this tab. We update the find text
  // _first_ since the FindBarView checks its emptiness to see if it should
  // clear the result count display when there's nothing in the box.
  find_bar_->SetFindTextAndSelectedRange(find_string,
                                         find_tab_helper->selected_range());
}

std::u16string FindBarController::GetSelectedText() {
  auto* host_view = web_contents()->GetRenderWidgetHostView();
  if (!host_view)
    return std::u16string();

  std::u16string selected_text = host_view->GetSelectedText();
  // This should be kept in sync with what TextfieldModel::Paste() does, since
  // that's what would run if the user explicitly pasted this text into the find
  // bar.
  base::TrimWhitespace(selected_text, base::TRIM_ALL, &selected_text);
  return selected_text;
}
