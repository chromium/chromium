// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_HANDLER_H_

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace privacy_sandbox {

class BaseDialogUIDelegate;

class BaseDialogHandler : public dialog::mojom::BaseDialogPageHandler {
 public:
  BaseDialogHandler(
      mojo::PendingReceiver<dialog::mojom::BaseDialogPageHandler> receiver,
      BaseDialogUIDelegate* delegate);

  BaseDialogHandler(const BaseDialogHandler&) = delete;
  BaseDialogHandler& operator=(const BaseDialogHandler&) = delete;

  ~BaseDialogHandler() override;

  // privacy_sandbox::dialog::mojom::BaseDialogPageHandler
  void ResizeDialog(uint32_t height) override;
  void ShowDialog() override;
  void CloseDialog() override;

 private:
  mojo::Receiver<dialog::mojom::BaseDialogPageHandler> receiver_;
  raw_ptr<BaseDialogUIDelegate> delegate_;
  bool has_resized = false;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_HANDLER_H_
