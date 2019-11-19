// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/find_bar/find_bar_platform_helper.h"
#include "chrome/browser/ui/find_bar/find_result_observer.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FindBar;
class Browser;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

enum class FindOnPageSelectionAction;
enum class FindBoxResultAction;

class FindBarController : public content::NotificationObserver,
                          public FindResultObserver {
 public:
  FindBarController(std::unique_ptr<FindBar> find_bar, Browser* browser);

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
  void EndFindSession(FindOnPageSelectionAction selection_action,
                      FindBoxResultAction result_action);

  // The visibility of the find bar view changed.
  void FindBarVisibilityChanged();

  // Accessor for the attached WebContents.
  content::WebContents* web_contents() const { return web_contents_; }

  // Changes the WebContents that this FindBar is attached to. This
  // occurs when the user switches tabs in the Browser window. |contents| can be
  // NULL.
  void ChangeWebContents(content::WebContents* contents);

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;

  void SetText(base::string16 text);

  // Called when the find text is updated in response to a user action.
  void OnUserChangedFindText(base::string16 text);

  Browser* browser() const { return browser_; }
  FindBar* find_bar() const { return find_bar_.get(); }

  // Reposition |view_location| such that it avoids |avoid_overlapping_rect|,
  // and return the new location.
  static gfx::Rect GetLocationForFindbarView(
      gfx::Rect view_location,
      const gfx::Rect& dialog_bounds,
      const gfx::Rect& avoid_overlapping_rect);

 private:
  // Sends an update to the find bar with the tab contents' current result. The
  // web_contents_ must be non-NULL before this call. This handles
  // de-flickering in addition to just calling the update function.
  void UpdateFindBarForCurrentResult();

  // For Windows and Linux this function sets the prepopulate text for the
  // Find text box. The propopulate value is the last value the user searched
  // for in the current tab, or (if blank) the last value searched for in any
  // tab. Mac has a global value for search, so this function does nothing on
  // Mac.
  void MaybeSetPrepopulateText();

  // Gets the text that is selected in the current tab, or an empty string.
  base::string16 GetSelectedText();

  content::NotificationRegistrar registrar_;

  std::unique_ptr<FindBar> find_bar_;

  // The WebContents we are currently associated with.  Can be NULL.
  content::WebContents* web_contents_ = nullptr;

  // The Browser creating this controller.
  Browser* const browser_;

  std::unique_ptr<FindBarPlatformHelper> find_bar_platform_helper_;

  // The last match count and ordinal we reported to the user. This is used
  // by UpdateFindBarForCurrentResult to avoid flickering.
  int last_reported_matchcount_ = 0;
  int last_reported_ordinal_ = 0;

  // If the user has changed the text in the find bar. Used to avoid
  // replacing user-entered text with selection.
  bool has_user_modified_text_ = false;

  ScopedObserver<FindTabHelper, FindResultObserver> find_tab_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(FindBarController);
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONTROLLER_H_
