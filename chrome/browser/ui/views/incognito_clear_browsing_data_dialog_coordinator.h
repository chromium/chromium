// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/incognito_clear_browsing_data_dialog_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/views/view_tracker.h"

class IncognitoClearBrowsingDataDialog;

// Handles the lifetime and showing/hidden state of the clear data dialog. Owned
// by the associated incognito browser.
class IncognitoClearBrowsingDataDialogCoordinator
    : public BrowserUserData<IncognitoClearBrowsingDataDialogCoordinator> {
 public:
  ~IncognitoClearBrowsingDataDialogCoordinator() override;

  // Shows the bubble for this browser anchored to the avatar toolbar button.
  void Show(IncognitoClearBrowsingDataDialogInterface::Type type);

  // Returns true if the bubble is currently showing for this window.
  bool IsShowing() const;

  IncognitoClearBrowsingDataDialog*
  GetIncognitoClearBrowsingDataDialogForTesting();

 private:
  friend class BrowserUserData<IncognitoClearBrowsingDataDialogCoordinator>;

  explicit IncognitoClearBrowsingDataDialogCoordinator(Browser* browser);

  views::ViewTracker bubble_tracker_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_COORDINATOR_H_
