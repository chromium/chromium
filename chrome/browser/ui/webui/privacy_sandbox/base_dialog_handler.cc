// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"

namespace privacy_sandbox {

using dialog::mojom::BaseDialogPage;
using dialog::mojom::BaseDialogPageHandler;
using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

BaseDialogHandler::BaseDialogHandler(
    mojo::PendingReceiver<BaseDialogPageHandler> receiver,
    mojo::PendingRemote<BaseDialogPage> page,
    DesktopViewManagerInterface* view_manager,
    BaseDialogUIDelegate* delegate)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
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

void BaseDialogHandler::EventOccurred(PrivacySandboxNotice notice,
                                      PrivacySandboxNoticeEvent event) {
  view_manager_->OnEventOccurred(notice, event);
}

void BaseDialogHandler::MaybeNavigateToNextStep(
    std::optional<PrivacySandboxNotice> next_id) {
  if (!delegate_) {
    return;
  }
  if (!next_id) {
    delegate_->CloseNativeView();
  } else {
    page_->NavigateToNextStep(*next_id);
    delegate_->SetPrivacySandboxNotice(*next_id);
  }
}

}  // namespace privacy_sandbox
