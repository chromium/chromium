// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_DIALOG_H_

#include <memory>

#include "ui/gfx/native_ui_types.h"

namespace views {
class DialogDelegate;
class View;
}  // namespace views

namespace crostini {

class AppRestartDialog {
 public:
  static void Show(int64_t display_id);
  static void ShowForTesting(gfx::NativeWindow context);

 private:
  static void ShowInternal(gfx::NativeWindow context);
  static std::unique_ptr<views::View> MakeCrostiniAppRestartView();
  static std::unique_ptr<views::DialogDelegate> MakeCrostiniAppRestartDelegate(
      std::unique_ptr<views::View> contents);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_DIALOG_H_
