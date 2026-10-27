// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FIND_IN_PAGE_FIND_TAB_HELPER_H_
#define COMPONENTS_FIND_IN_PAGE_FIND_TAB_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/find_in_page/find_notification_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/range/range.h"

namespace find_in_page {

class FindResultObserver;
enum class SelectionAction;

// Per-tab find manager. Handles dealing with the life cycle of find sessions.
class FindTabHelper : public content::WebContentsUserData<FindTabHelper>,
                      public content::WebContentsObserver {
 public:
  // The delegate tracks search text state.
  class Delegate {
   public:
    // Informs the delegate when the user searches.
    virtual void SetLastSearchText(const std::u16string& text,
                                   content::WebContents* web_contents) = 0;

    // Gets the text to prepopulate into the search field for new searches. May
    // return an empty string.
    virtual std::u16string GetSearchPrepopulateText(
        content::WebContents* web_contents) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  FindTabHelper(const FindTabHelper&) = delete;
  FindTabHelper& operator=(const FindTabHelper&) = delete;
  ~FindTabHelper() override;

  void AddObserver(FindResultObserver* observer);
  void RemoveObserver(FindResultObserver* observer);

  // Starts a new find operation or advances to the next/previous match for
  // `search_string` in the associated `WebContents`:
  // - If `search_string` differs from the current search, this starts a new
  //   find session (scoping all matches across frames).
  // - If `search_string` matches the current search, this advances the active
  //   match selection forward or backward without re-scoping.
  //
  // This function does not block; results are delivered asynchronously to
  // registered `FindResultObserver`s via `OnFindResultAvailable()`.
  //
  // `find_match`: Controls whether the renderer should locate and activate a
  // match (highlighting it and optionally scrolling to it) or only perform
  // background scoping (match counts and highlighting without selecting an
  // active match). Passed as false during passive pre-population on Find bar
  // open or when syncing the macOS find pasteboard. Stored in
  // `should_find_match_`.
  //
  // `scroll_to_match`: Controls whether the viewport should scroll to the
  // activated match. Only applicable when `find_match` is true.
  //
  // `skip_viewport`: Controls whether find-next/previous should skip matches
  // currently visible in the viewport and jump to the first match outside the
  // viewport.
  void StartFinding(std::u16string search_string,
                    bool forward_direction,
                    bool case_sensitive,
                    bool find_match,
                    bool scroll_to_match = true,
                    bool run_synchronously_for_testing = false,
                    bool skip_viewport = false);

  // Stops the current Find operation.
  void StopFinding(SelectionAction selection_action);

  // When the user commits to a search query or jumps from one result
  // to the next, move accessibility focus to the next find result.
  void ActivateFindInPageResultForAccessibility();

  // Retrieves the starting text for searching in the tab.
  std::u16string GetInitialSearchText();

  // Accessors/Setters for `find_ui_active_`.
  bool find_ui_active() const { return find_ui_active_; }
  void set_find_ui_active(bool find_ui_active) {
    find_ui_active_ = find_ui_active;
  }

  // Accessors/Setters for `find_ui_focused_`.
  bool find_ui_focused() const { return find_ui_focused_; }
  void set_find_ui_focused(bool find_ui_focused) {
    find_ui_focused_ = find_ui_focused;
  }

  // Used _only_ by testing to get the current request ID.
  int current_find_request_id() { return current_find_request_id_; }

  // Accessor for `find_text_`. Used to determine if this WebContents has any
  // active searches.
  std::u16string find_text() const { return find_text_; }

  // Accessor for `previous_find_text_`.
  std::u16string previous_find_text() const { return previous_find_text_; }

  // Accessor for the search text of the last completed search where an active
  // match was requested (i.e., where `should_find_match_` was true and
  // `final_update` was true (all frames finished scoping)).
  std::u16string last_completed_find_text() const {
    return last_completed_find_text_;
  }

  void set_last_completed_find_text(
      const std::u16string& last_completed_find_text) {
    last_completed_find_text_ = last_completed_find_text;
  }

  gfx::Range selected_range() const { return selected_range_; }
  void set_selected_range(const gfx::Range& selected_range) {
    selected_range_ = selected_range;
  }

  // Accessor for `last_search_result_`.
  const FindNotificationDetails& find_result() const {
    return last_search_result_;
  }

  bool should_find_match() const { return should_find_match_; }

#if BUILDFLAG(IS_ANDROID)
  // Selects and zooms to the find result nearest to the point (x, y)
  // defined in find-in-page coordinates.
  void ActivateNearestFindResult(float x, float y);

  // Asks the renderer to send the rects of the current find matches.
  void RequestFindMatchRects(int current_version);
#endif

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Used to determine if the find session is active for this WebContents.
  bool is_find_session_active() const { return is_find_session_active_; }

 private:
  explicit FindTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<FindTabHelper>;

  // True if the Find UI is active for this tab.
  bool find_ui_active_ = false;

  // True if the Find UI is focused for this tab.
  bool find_ui_focused_ = false;

  // True if a find operation was aborted. This can happen if the Find box is
  // closed or if the search term inside the Find box is erased while a search
  // is in progress. This can also be set if a page has been reloaded, and will
  // on a find-next operation result in a full find operation so that the
  // highlighting for inactive matches can be repainted.
  bool find_op_aborted_ = false;

  // This variable keeps track of what the most recent request ID is. -1
  // (never a real request ID) means no find request has been made yet.
  int current_find_request_id_ = -1;

  // This variable keeps track of the ID of the first find request in the
  // current session, which also uniquely identifies the session. -1 (never a
  // real request ID) means no session has started yet.
  int current_find_session_id_ = -1;

  // The current string we are, or just finished, searching for. This is used to
  // determine if this is a find or a find-next operation (find-next should not
  // increase the request ID).
  std::u16string find_text_;

  // The string we searched for before `find_text_`.
  std::u16string previous_find_text_;

  // Used to keep track of the search text of the last completed search where an
  // active match was requested. A single find session can result in multiple
  // final updates, if the document contents change dynamically. It's a nuisance
  // to notify the user more than once that a search came up empty, and we never
  // want to notify the user that a previously successful search's results were
  // removed because, for instance, the page is being torn down during
  // navigation.
  std::u16string last_completed_find_text_;

  // The selection within the text.
  gfx::Range selected_range_;

  // Whether the last search was case sensitive or not.
  bool last_search_case_sensitive_ = false;

  // The last find result. This object contains details about the number of
  // matches, the find selection rectangle, etc. The UI can access this
  // information to build its presentation.
  FindNotificationDetails last_search_result_;

  // The value of the `find_match` option for the active search. False if
  // there is no active search.
  bool should_find_match_ = false;

  // Whether a find session is active for this WebContents.
  bool is_find_session_active_ = false;

  // The optional delegate that remembers recent search text state.
  raw_ptr<Delegate> delegate_ = nullptr;

  base::ObserverList<FindResultObserver> observers_;

  // content::WebContentsObserver:
  // Receives search replies from WebContents. `final_update` indicates
  // whether all frames in the page have finished scoping (whether this is the
  // final result for `request_id`).
  void DidReceiveFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace find_in_page

#endif  // COMPONENTS_FIND_IN_PAGE_FIND_TAB_HELPER_H_
