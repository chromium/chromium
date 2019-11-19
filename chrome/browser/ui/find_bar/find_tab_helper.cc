// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_tab_helper.h"

#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/find_bar/find_result_observer.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "ui/gfx/geometry/rect_f.h"

using content::WebContents;

// static
int FindTabHelper::find_request_id_counter_ = -1;

FindTabHelper::FindTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      find_ui_active_(false),
      find_op_aborted_(false),
      current_find_request_id_(find_request_id_counter_++),
      current_find_session_id_(current_find_request_id_),
      last_search_case_sensitive_(false),
      last_search_result_() {
}

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
                                 bool run_synchronously_for_testing) {
  // Remove the carriage return character, which generally isn't in web content.
  const base::char16 kInvalidChars[] = { '\r', 0 };
  base::RemoveChars(search_string, kInvalidChars, &search_string);

  // If search_string is empty, it means FindNext was pressed with a keyboard
  // shortcut so unless we have something to search for we return early.
  if (search_string.empty() && find_text_.empty()) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    base::string16 last_search_prepopulate_text =
        FindBarStateFactory::GetLastPrepopulateText(profile);

    // Try the last thing we searched for on this tab, then the last thing
    // searched for on any tab.
    if (!previous_find_text_.empty())
      search_string = previous_find_text_;
    else if (!last_search_prepopulate_text.empty())
      search_string = last_search_prepopulate_text;
    else
      return;
  }

  // Keep track of the previous search.
  previous_find_text_ = find_text_;

  // This is a FindNext operation if we are searching for the same text again,
  // or if the passed in search text is empty (FindNext keyboard shortcut). The
  // exception to this is if the Find was aborted (then we don't want FindNext
  // because the highlighting has been cleared and we need it to reappear). We
  // therefore treat FindNext after an aborted Find operation as a full fledged
  // Find.
  bool find_next = (find_text_ == search_string || search_string.empty()) &&
                   (last_search_case_sensitive_ == case_sensitive) &&
                   !find_op_aborted_;

  current_find_request_id_ = find_request_id_counter_++;
  if (!find_next)
    current_find_session_id_ = current_find_request_id_;

  if (!search_string.empty())
    find_text_ = search_string;
  last_search_case_sensitive_ = case_sensitive;

  find_op_aborted_ = false;

  // Keep track of what the last search was across the tabs.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  FindBarState* find_bar_state = FindBarStateFactory::GetForProfile(profile);
  find_bar_state->set_last_prepopulate_text(find_text_);

  auto options = blink::mojom::FindOptions::New();
  options->forward = forward_direction;
  options->match_case = case_sensitive;
  options->find_next = find_next;
  options->run_synchronously_for_testing = run_synchronously_for_testing;
  web_contents()->Find(current_find_request_id_, find_text_,
                       std::move(options));
}

void FindTabHelper::StopFinding(FindOnPageSelectionAction selection_action) {
  if (selection_action == FindOnPageSelectionAction::kClear) {
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

  content::StopFindAction action;
  switch (selection_action) {
    case FindOnPageSelectionAction::kClear:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case FindOnPageSelectionAction::kKeep:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case FindOnPageSelectionAction::kActivate:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      NOTREACHED();
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }
  web_contents()->StopFinding(action);
}

void FindTabHelper::ActivateFindInPageResultForAccessibility() {
  web_contents()->GetMainFrame()->ActivateFindInPageResultForAccessibility(
      current_find_request_id_);
}

#if defined(OS_ANDROID)
void FindTabHelper::ActivateNearestFindResult(float x, float y) {
  if (!find_op_aborted_ && !find_text_.empty()) {
    web_contents()->ActivateNearestFindResult(x, y);
  }
}

void FindTabHelper::RequestFindMatchRects(int current_version) {
  if (!find_op_aborted_ && !find_text_.empty())
    web_contents()->RequestFindMatchRects(current_version);
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
    last_search_result_ = FindNotificationDetails(
        request_id, number_of_matches, selection, active_match_ordinal,
        final_update);
    for (auto& observer : observers_)
      observer.OnFindResultAvailable(web_contents());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FindTabHelper)
