// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"

namespace privacy_sandbox {

using dialog::mojom::BaseDialogPageHandler;
using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

BaseDialogHandler::BaseDialogHandler(
    mojo::PendingReceiver<BaseDialogPageHandler> receiver,
    DesktopViewManagerInterface* view_manager,
    BaseDialogUIDelegate* delegate)
    : receiver_(this, std::move(receiver)),
      delegate_(delegate),
      view_manager_(view_manager) {
  CHECK(view_manager_);
  desktop_view_manager_observation_.Observe(view_manager_);
}

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

void BaseDialogHandler::EventOccurred(PrivacySandboxNotice notice,
                                      PrivacySandboxNoticeEvent event) {
  view_manager_->OnEventOccurred(notice, event);
}

void BaseDialogHandler::MaybeNavigateToNextStep(
    std::optional<PrivacySandboxNotice> next_id) {
  // TODO(crbug.com/408016824): implement and add tests.
}

}  // namespace privacy_sandbox
