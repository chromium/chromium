// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/incognito_clear_browsing_data_dialog_interface.h"
#include "ui/views/view_tracker.h"

class BrowserUserEducationInterface;
class BrowserWindowInterface;
class IncognitoClearBrowsingDataDialog;
class Profile;

// Handles the lifetime and showing/hidden state of the clear data dialog. Owned
// by the associated incognito browser.
class IncognitoClearBrowsingDataDialogCoordinator {
 public:
  explicit IncognitoClearBrowsingDataDialogCoordinator(
      BrowserWindowInterface* browser);
  IncognitoClearBrowsingDataDialogCoordinator(
      const IncognitoClearBrowsingDataDialogCoordinator&) = delete;
  IncognitoClearBrowsingDataDialogCoordinator& operator=(
      const IncognitoClearBrowsingDataDialogCoordinator&) = delete;
  ~IncognitoClearBrowsingDataDialogCoordinator();

  // Shows the bubble for this browser anchored to the avatar toolbar button.
  void Show(IncognitoClearBrowsingDataDialogInterface::Type type);

  // Returns true if the bubble is currently showing for this window.
  bool IsShowing() const;

  IncognitoClearBrowsingDataDialog*
  GetIncognitoClearBrowsingDataDialogForTesting();

 private:
  views::ViewTracker bubble_tracker_;

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<BrowserUserEducationInterface> user_education_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
