// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/find_in_page/find_tab_helper.h"

#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "ui/gfx/geometry/rect_f.h"

using content::WebContents;

namespace find_in_page {

// static
int FindTabHelper::find_request_id_counter_ = -1;

FindTabHelper::FindTabHelper(WebContents* web_contents)
    : web_contents_(web_contents),
      current_find_request_id_(find_request_id_counter_++),
      current_find_session_id_(current_find_request_id_) {}

FindTabHelper::~FindTabHelper() {
  for (auto& observer : observers_)
    observer.OnFindTabHelperDestroyed(this);
}

void FindTabHelper::AddObserver(FindResultObserver* observer) {
  observers_.AddObserver(observer);
}

void FindTabHelper::RemoveObserver(FindResultObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FindTabHelper::StartFinding(base::string16 search_string,
                                 bool forward_direction,
                                 bool case_sensitive,
                                 bool find_match,
                                 bool run_synchronously_for_testing) {
  // Remove the carriage return character, which generally isn't in web content.
  const base::char16 kInvalidChars[] = {'\r', 0};
  base::RemoveChars(search_string, kInvalidChars, &search_string);

  // If search_string is empty, it means FindNext was pressed with a keyboard
  // shortcut so unless we have something to search for we return early.
  if (search_string.empty() && find_text_.empty()) {
    search_string = GetInitialSearchText();

    if (search_string.empty())
      return;
  }

  // NB: search_string will be empty when using the FindNext keyboard shortcut.
  bool new_session = (find_text_ != search_string && !search_string.empty()) ||
                     (last_search_case_sensitive_ != case_sensitive) ||
                     find_op_aborted_;

  // Continuing here would just find the same results, potentially causing
  // some flicker in the highlighting.
  if (!new_session && !find_match)
    return;

  // Keep track of the previous search.
  previous_find_text_ = find_text_;

  current_find_request_id_ = find_request_id_counter_++;
  if (new_session)
    current_find_session_id_ = current_find_request_id_;

  if (!search_string.empty())
    find_text_ = search_string;
  last_search_case_sensitive_ = case_sensitive;

  find_op_aborted_ = false;
  should_find_match_ = find_match;

  // Keep track of what the last search was across the tabs.
  if (delegate_)
    delegate_->SetLastSearchText(find_text_);

  auto options = blink::mojom::FindOptions::New();
  options->forward = forward_direction;
  options->match_case = case_sensitive;
  options->new_session = new_session;
  options->find_match = find_match;
  options->run_synchronously_for_testing = run_synchronously_for_testing;
  web_contents_->Find(current_find_request_id_, find_text_, std::move(options));
}

void FindTabHelper::StopFinding(SelectionAction selection_action) {
  if (selection_action == SelectionAction::kClear) {
    // kClearSelection means the find string has been cleared by the user, but
    // the UI has not been dismissed. In that case we want to clear the
    // previously remembered search (http://crbug.com/42639).
    previous_find_text_ = base::string16();
  } else {
    find_ui_active_ = false;
    if (!find_text_.empty())
      previous_find_text_ = find_text_;
  }
  find_text_.clear();
  last_completed_find_text_.clear();
  find_op_aborted_ = true;
  last_search_result_ = FindNotificationDetails();
  should_find_match_ = false;

  content::StopFindAction action;
  switch (selection_action) {
    case SelectionAction::kClear:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case SelectionAction::kKeep:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case SelectionAction::kActivate:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      NOTREACHED();
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }
  web_contents_->StopFinding(action);
}

void FindTabHelper::ActivateFindInPageResultForAccessibility() {
  web_contents_->GetMainFrame()->ActivateFindInPageResultForAccessibility(
      current_find_request_id_);
}

base::string16 FindTabHelper::GetInitialSearchText() {
  // Try the last thing we searched for in this tab.
  if (!previous_find_text_.empty())
    return previous_find_text_;

  // Then defer to the delegate.
  return delegate_ ? delegate_->GetSearchPrepopulateText() : base::string16();
}

#if defined(OS_ANDROID)
void FindTabHelper::ActivateNearestFindResult(float x, float y) {
  if (!find_op_aborted_ && !find_text_.empty()) {
    web_contents_->ActivateNearestFindResult(x, y);
  }
}

void FindTabHelper::RequestFindMatchRects(int current_version) {
  if (!find_op_aborted_ && !find_text_.empty())
    web_contents_->RequestFindMatchRects(current_version);
}
#endif

void FindTabHelper::HandleFindReply(int request_id,
                                    int number_of_matches,
                                    const gfx::Rect& selection_rect,
                                    int active_match_ordinal,
                                    bool final_update) {
  // Ignore responses for requests that have been aborted.
  // Ignore responses for requests from previous sessions. That way we won't act
  // on stale results when the user has already typed in another query.
  if (!find_op_aborted_ && request_id >= current_find_session_id_) {
    if (number_of_matches == -1)
      number_of_matches = last_search_result_.number_of_matches();
    if (active_match_ordinal == -1)
      active_match_ordinal = last_search_result_.active_match_ordinal();

    gfx::Rect selection = selection_rect;
    if (final_update && active_match_ordinal == 0)
      selection = gfx::Rect();
    else if (selection_rect.IsEmpty())
      selection = last_search_result_.selection_rect();

    // Notify the UI, automation and any other observers that a find result was
    // found.
    last_search_result_ =
        FindNotificationDetails(request_id, number_of_matches, selection,
                                active_match_ordinal, final_update);
    for (auto& observer : observers_)
      observer.OnFindResultAvailable(web_contents_);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FindTabHelper)

}  // namespace find_in_page
