// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/incognito_clear_browsing_data_dialog_interface.h"
#include "ui/views/view_tracker.h"

class BrowserUserEducationInterface;
class IncognitoClearBrowsingDataDialog;
class Profile;

namespace views {
class View;
}  // namespace views

// Handles the lifetime and showing/hidden state of the clear data dialog. Owned
// by the associated incognito browser.
class IncognitoClearBrowsingDataDialogCoordinator {
 public:
  explicit IncognitoClearBrowsingDataDialogCoordinator(Profile* profile);
  IncognitoClearBrowsingDataDialogCoordinator(
      const IncognitoClearBrowsingDataDialogCoordinator&) = delete;
  IncognitoClearBrowsingDataDialogCoordinator& operator=(
      const IncognitoClearBrowsingDataDialogCoordinator&) = delete;
  ~IncognitoClearBrowsingDataDialogCoordinator();

  // Shows the bubble for this browser anchored to `anchor_view`.
  void Show(IncognitoClearBrowsingDataDialogInterface::Type type,
            views::View* anchor_view);

  // Returns true if the bubble is currently showing for this window.
  bool IsShowing() const;

  IncognitoClearBrowsingDataDialog*
  GetIncognitoClearBrowsingDataDialogForTesting();

 private:
  views::ViewTracker bubble_tracker_;
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
