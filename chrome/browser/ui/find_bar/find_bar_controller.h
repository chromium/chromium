// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONTROLLER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/find_in_page/find_tab_helper.h"
#include "content/public/browser/web_contents_observer.h"

class FindBar;
class FindBarPlatformHelper;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace find_in_page {
enum class SelectionAction;
enum class ResultAction;
}  // namespace find_in_page

class FindBarController : public content::WebContentsObserver,
                          public find_in_page::FindResultObserver {
 public:
  explicit FindBarController(std::unique_ptr<FindBar> find_bar);

  FindBarController(const FindBarController&) = delete;
  FindBarController& operator=(const FindBarController&) = delete;

  ~FindBarController() override;

  // Shows the find bar. Any previous search string will again be visible.
  // The find operation will also be started depending on |find_next| and
  // if there is currently a text selection. |find_next| means the user
  // used a command to advance the search and |forward_direction| indicates if
  // the find should be forward or backwards.
  void Show(bool find_next = false, bool forward_direction = true);

  // Ends the current session. |selection_action| specifies what to do with the
  // selection on the page created by the find operation. |result_action|
  // specifies what to do with the contents of the Find box (after ending).
  void EndFindSession(find_in_page::SelectionAction selection_action,
                      find_in_page::ResultAction result_action);

  // Changes the WebContents that this FindBar is attached to. This
  // occurs when the user switches tabs in the Browser window. |contents| can be
  // NULL.
  void ChangeWebContents(content::WebContents* contents);

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // find_in_page::FindResultObserver:
  void OnFindEmptyText(content::WebContents* web_contents) override;
  void OnFindResultAvailable(content::WebContents* web_contents) override;

  void SetText(std::u16string text);

  // Called when the find text is updated in response to a user action.
  void OnUserChangedFindText(std::u16string text);

  FindBar* find_bar() const { return find_bar_.get(); }

 private:
  // Sends an update to the find bar with the tab contents' current result. The
  // `web_contents()` must be non-NULL before this call. This handles
  // de-flickering in addition to just calling the update function.
  void UpdateFindBarForCurrentResult();

  // For Windows and Linux this function sets the prepopulate text for the
  // Find text box. The propopulate value is the last value the user searched
  // for in the current tab, or (if blank) the last value searched for in any
  // tab. Mac has a global value for search, so this function does nothing on
  // Mac.
  void MaybeSetPrepopulateText();

  // Gets the text that is selected in the current tab, or an empty string.
  std::u16string GetSelectedText();

  std::unique_ptr<FindBar> find_bar_;

  std::unique_ptr<FindBarPlatformHelper> find_bar_platform_helper_;

  // The last match count and ordinal we reported to the user. This is used
  // by UpdateFindBarForCurrentResult to avoid flickering.
  int last_reported_matchcount_ = 0;
  int last_reported_ordinal_ = 0;

  // If the user has changed the text in the find bar. Used to avoid
  // replacing user-entered text with selection.
  bool has_user_modified_text_ = false;

  base::ScopedObservation<find_in_page::FindTabHelper,
                          find_in_page::FindResultObserver>
      find_tab_observation_{this};
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONTROLLER_H_
