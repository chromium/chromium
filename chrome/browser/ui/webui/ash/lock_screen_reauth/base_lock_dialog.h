// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_BASE_LOCK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_BASE_LOCK_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace ash {

// A modal system dialog without any frame decorating it.
class BaseLockDialog : public SystemWebDialogDelegate {
 protected:
  BaseLockDialog(const GURL& url, const gfx::Size& desired_size);
  BaseLockDialog(BaseLockDialog const&) = delete;
  BaseLockDialog& operator=(const BaseLockDialog&) = delete;
  ~BaseLockDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  ui::mojom::ModalType GetDialogModalType() const override;

  static constexpr gfx::Size kBaseLockDialogSize = gfx::Size(768, 640);

 private:
  gfx::Size desired_size_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_BASE_LOCK_DIALOG_H_
