// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_DIALOG_H_

#include "ui/gfx/native_widget_types.h"

namespace crostini {

void ShowAppRestartDialog(int64_t display_id);
void ShowAppRestartDialogForTesting(gfx::NativeWindow context);

}  // namespace crostini

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_RESTART_DIALOG_H_
