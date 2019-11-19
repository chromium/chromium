// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/color_chooser_dialog.h"

#include <commdlg.h>

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/win/hwnd_util.h"

using content::BrowserThread;

// static
COLORREF ColorChooserDialog::g_custom_colors[16];

ColorChooserDialog::ColorChooserDialog(views::ColorChooserListener* listener)
    : listener_(listener) {
  DCHECK(listener_);
  CopyCustomColors(g_custom_colors, custom_colors_);
}

void ColorChooserDialog::Open(SkColor initial_color,
                              gfx::NativeWindow owning_window) {
  HWND owning_hwnd = views::HWNDForNativeWindow(owning_window);

  std::unique_ptr<RunState> run_state = BeginRun(owning_hwnd);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      run_state->dialog_task_runner;
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&ColorChooserDialog::ExecuteOpen, this,
                                       initial_color, std::move(run_state)));
}

bool ColorChooserDialog::IsRunning(gfx::NativeWindow owning_window) const {
  return listener_ && IsRunningDialogForOwner(
      views::HWNDForNativeWindow(owning_window));
}

void ColorChooserDialog::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = NULL;
}

ColorChooserDialog::~ColorChooserDialog() {
}

void ColorChooserDialog::ExecuteOpen(SkColor color,
                                     std::unique_ptr<RunState> run_state) {
  CHOOSECOLOR cc;
  cc.lStructSize = sizeof(CHOOSECOLOR);
  cc.hwndOwner = run_state->owner;
  cc.rgbResult = skia::SkColorToCOLORREF(color);
  cc.lpCustColors = custom_colors_;
  cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
  bool success = !!ChooseColor(&cc);
  DisableOwner(cc.hwndOwner);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&ColorChooserDialog::DidCloseDialog, this,
                                success, skia::COLORREFToSkColor(cc.rgbResult),
                                std::move(run_state)));
}

void ColorChooserDialog::DidCloseDialog(bool chose_color,
                                        SkColor color,
                                        std::unique_ptr<RunState> run_state) {
  EndRun(std::move(run_state));
  CopyCustomColors(custom_colors_, g_custom_colors);
  if (listener_) {
    if (chose_color)
      listener_->OnColorChosen(color);
    listener_->OnColorChooserDialogClosed();
  }
}

void ColorChooserDialog::CopyCustomColors(COLORREF* src, COLORREF* dst) {
  memcpy(dst, src, sizeof(COLORREF) * base::size(g_custom_colors));
}
