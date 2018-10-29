// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPGRADE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPGRADE_VIEW_H_

#include "ui/views/window/dialog_delegate.h"

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

class Profile;

// Provides a warning to the user that an upgrade is required and and internet
// connection is needed.
class CrostiniUpgradeView : public views::DialogDelegateView {
 public:
  static void Show(Profile* profile);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  static CrostiniUpgradeView* GetActiveViewForTesting();

 private:
  CrostiniUpgradeView();
  ~CrostiniUpgradeView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UPGRADE_VIEW_H_
