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
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/range/range.h"

namespace find_in_page {

class FindResultObserver;
enum class SelectionAction;

// Per-tab find manager. Handles dealing with the life cycle of find sessions.
class FindTabHelper : public content::WebContentsUserData<FindTabHelper> {
 public:
  // The delegate tracks search text state.
  class Delegate {
   public:
    // Informs the delegate when the user searches.
    virtual void SetLastSearchText(const std::u16string& text) = 0;

    // Gets the text to prepopulate into the search field for new searches. May
    // return an empty string.
    virtual std::u16string GetSearchPrepopulateText() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  FindTabHelper(const FindTabHelper&) = delete;
  FindTabHelper& operator=(const FindTabHelper&) = delete;
  ~FindTabHelper() override;

  void AddObserver(FindResultObserver* observer);
  void RemoveObserver(FindResultObserver* observer);

  // Starts the Find operation by calling StartFinding on the Tab. This function
  // can be called from the outside as a result of hot-keys, so it uses the
  // last remembered search string as specified with set_find_string(). This
  // function does not block while a search is in progress. The controller will
  // receive the results through the notification mechanism. See Observe(...)
  // for details.
  //
  // |find_match| controls whether to find the first match or to only do match
  // counts and highlighting.
  void StartFinding(std::u16string search_string,
                    bool forward_direction,
                    bool case_sensitive,
                    bool find_match,
                    bool run_synchronously_for_testing = false);

  // Stops the current Find operation.
  void StopFinding(SelectionAction selection_action);

  // When the user commits to a search query or jumps from one result
  // to the next, move accessibility focus to the next find result.
  void ActivateFindInPageResultForAccessibility();

  // Retrieves the starting text for searching in the tab.
  std::u16string GetInitialSearchText();

  // Accessors/Setters for find_ui_active_.
  bool find_ui_active() const { return find_ui_active_; }
  void set_find_ui_active(bool find_ui_active) {
    find_ui_active_ = find_ui_active;
  }

  // Used _only_ by testing to get the current request ID.
  int current_find_request_id() { return current_find_request_id_; }

  // Accessor for find_text_. Used to determine if this WebContents has any
  // active searches.
  std::u16string find_text() const { return find_text_; }

  // Accessor for the previous search we issued.
  std::u16string previous_find_text() const { return previous_find_text_; }

  // Accessor for the last completed search (i.e., where |find_match| was true
  // and we got a final_update result).
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

  // Accessor for find_result_.
  const FindNotificationDetails& find_result() const {
    return last_search_result_;
  }

  bool should_find_match() const { return should_find_match_; }

#if BUILDFLAG(IS_ANDROID)
  // Selects and zooms to the find result nearest to the point (x,y)
  // defined in find-in-page coordinates.
  void ActivateNearestFindResult(float x, float y);

  // Asks the renderer to send the rects of the current find matches.
  void RequestFindMatchRects(int current_version);
#endif

  void HandleFindReply(int request_id,
                       int number_of_matches,
                       const gfx::Rect& selection_rect,
                       int active_match_ordinal,
                       bool final_update);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  explicit FindTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<FindTabHelper>;

  // Each time a search request comes in we assign it an id before passing it
  // over the IPC so that when the results come in we can evaluate whether we
  // still care about the results of the search (in some cases we don't because
  // the user has issued a new search).
  static int find_request_id_counter_;

  // True if the Find UI is active for this Tab.
  bool find_ui_active_ = false;

  // True if a Find operation was aborted. This can happen if the Find box is
  // closed or if the search term inside the Find box is erased while a search
  // is in progress. This can also be set if a page has been reloaded, and will
  // on FindNext result in a full Find operation so that the highlighting for
  // inactive matches can be repainted.
  bool find_op_aborted_ = false;

  // This variable keeps track of what the most recent request ID is.
  int current_find_request_id_;

  // This variable keeps track of the ID of the first find request in the
  // current session, which also uniquely identifies the session.
  int current_find_session_id_;

  // The current string we are/just finished searching for. This is used to
  // figure out if this is a Find or a FindNext operation (FindNext should not
  // increase the request id).
  std::u16string find_text_;

  // The string we searched for before |find_text_|.
  std::u16string previous_find_text_;

  // Used to keep track the last completed search (i.e., where |find_match|
  // was true and we got a final_update result). A single find session can
  // result in multiple final updates, if the document contents change
  // dynamically. It's a nuisance to notify the user more than once that a
  // search came up empty, and we never want to notify the user that a
  // previously successful search's results were removed because,
  // for instance, the page is being torn down during navigation.
  std::u16string last_completed_find_text_;

  // The selection within the text.
  gfx::Range selected_range_;

  // Whether the last search was case sensitive or not.
  bool last_search_case_sensitive_ = false;

  // The last find result. This object contains details about the number of
  // matches, the find selection rectangle, etc. The UI can access this
  // information to build its presentation.
  FindNotificationDetails last_search_result_;

  // The value of the |find_match| option for the active search, or false if
  // there is no active search.
  bool should_find_match_ = false;

  // The optional delegate that remembers recent search text state.
  raw_ptr<Delegate> delegate_ = nullptr;

  base::ObserverList<FindResultObserver> observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace find_in_page

#endif  // COMPONENTS_FIND_IN_PAGE_FIND_TAB_HELPER_H_
