// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPDATE_FILESYSTEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPDATE_FILESYSTEM_VIEW_H_

#include "chrome/browser/ash/crostini/crostini_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace crostini {

// Shows the Crostini Container Upgrade dialog (for running upgrades in the
// container).
void ShowCrostiniUpdateFilesystemView(Profile* profile,
                                      CrostiniUISurface ui_surface);

// Show the Crostini Container Upgrade dialog after a delay
// (CloseCrostiniUpdateFilesystemView will cancel the next dialog show).
void PrepareShowCrostiniUpdateFilesystemView(Profile* profile,
                                             CrostiniUISurface ui_surface);

// Closes the current CrostiniUpdateFilesystemView or ensures that the view will
// not open until PrepareShowCrostiniUpdateFilesystemView is called again.
void CloseCrostiniUpdateFilesystemView();

void SetCrostiniUpdateFilesystemSkipDelayForTesting(bool should_skip);

}  // namespace crostini

// Provides a warning to the user that an upgrade is occurring and Crostini
// start will take longer than usual.
class CrostiniUpdateFilesystemView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(CrostiniUpdateFilesystemView, views::BubbleDialogDelegateView)

 public:
  static void Show(Profile* profile);

  static CrostiniUpdateFilesystemView* GetActiveViewForTesting();

 private:
  CrostiniUpdateFilesystemView();
  ~CrostiniUpdateFilesystemView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPDATE_FILESYSTEM_VIEW_H_
