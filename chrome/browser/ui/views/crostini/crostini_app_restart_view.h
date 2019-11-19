// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_VIEW_H_

#include "ash/public/cpp/shelf_types.h"
#include "ui/views/window/dialog_delegate.h"

// Provide user a choice to restart the app after display density change.
class CrostiniAppRestartView : public views::DialogDelegateView {
 public:
  // Create and show a new dialog.
  static void Show(const ash::ShelfID& id, int64_t display_id);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;

 private:
  CrostiniAppRestartView(const ash::ShelfID& id, int64_t display_id);
  ~CrostiniAppRestartView() override = default;

  ash::ShelfID id_;
  int64_t display_id_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppRestartView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_VIEW_H_
