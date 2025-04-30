// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"

namespace privacy_sandbox {

using dialog::mojom::BaseDialogPageHandler;

BaseDialogHandler::BaseDialogHandler(
    mojo::PendingReceiver<BaseDialogPageHandler> receiver,
    BaseDialogUIDelegate* delegate)
    : receiver_(this, std::move(receiver)), delegate_(delegate) {}

BaseDialogHandler::~BaseDialogHandler() = default;

void BaseDialogHandler::ResizeDialog(uint32_t height) {
  if (!delegate_) {
    return;
  }
  CHECK(!has_resized);
  delegate_->ResizeNativeView(height);
  has_resized = true;
}

void BaseDialogHandler::ShowDialog() {
  if (!delegate_) {
    return;
  }
  delegate_->ShowNativeView();
}

void BaseDialogHandler::CloseDialog() {
  if (!delegate_) {
    return;
  }
  delegate_->CloseNativeView();
}

}  // namespace privacy_sandbox
