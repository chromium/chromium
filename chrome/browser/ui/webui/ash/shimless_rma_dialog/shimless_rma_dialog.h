// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SHIMLESS_RMA_DIALOG_SHIMLESS_RMA_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SHIMLESS_RMA_DIALOG_SHIMLESS_RMA_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShimlessRmaDialog : public SystemWebDialogDelegate,
                          public display::DisplayObserver {
 public:
  static void ShowDialog();

 protected:
  ShimlessRmaDialog();
  ~ShimlessRmaDialog() override;

  ShimlessRmaDialog(const ShimlessRmaDialog&) = delete;
  ShimlessRmaDialog& operator=(const ShimlessRmaDialog&) = delete;

  // SystemWebDialogDelegate
  std::string Id() override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldCloseDialogOnEscape() const override;
  bool CanMaximizeDialog() const override;

 private:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  const std::string id_ = "shimless-rma-dialog";
  display::ScopedDisplayObserver display_observer_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SHIMLESS_RMA_DIALOG_SHIMLESS_RMA_DIALOG_H_
