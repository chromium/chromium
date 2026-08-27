// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/incognito_clear_browsing_data_dialog_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_tracker.h"

class BrowserWindowInterface;
class IncognitoClearBrowsingDataDialog;
class Profile;

// Handles the lifetime and showing/hidden state of the clear data dialog. Owned
// by the associated incognito browser.
class IncognitoClearBrowsingDataDialogCoordinator {
 public:
  DECLARE_USER_DATA(IncognitoClearBrowsingDataDialogCoordinator);

  // `host` is the UnownedUserDataHost of the browser window this coordinator
  // serves.
  IncognitoClearBrowsingDataDialogCoordinator(Profile* profile,
                                              ui::UnownedUserDataHost& host);

  // Returns the coordinator for `browser`, or null if it does not have one.
  static IncognitoClearBrowsingDataDialogCoordinator* From(
      BrowserWindowInterface* browser);
  IncognitoClearBrowsingDataDialogCoordinator(
      const IncognitoClearBrowsingDataDialogCoordinator&) = delete;
  IncognitoClearBrowsingDataDialogCoordinator& operator=(
      const IncognitoClearBrowsingDataDialogCoordinator&) = delete;
  ~IncognitoClearBrowsingDataDialogCoordinator();

  // Shows the bubble for this browser anchored to `anchor`.
  void Show(IncognitoClearBrowsingDataDialogInterface::Type type,
            views::BubbleAnchor anchor);

  // Returns true if the bubble is currently showing for this window.
  bool IsShowing() const;

  IncognitoClearBrowsingDataDialog*
  GetIncognitoClearBrowsingDataDialogForTesting();

 private:
  ui::ScopedUnownedUserData<IncognitoClearBrowsingDataDialogCoordinator>
      scoped_unowned_user_data_;

  views::ViewTracker bubble_tracker_;
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
