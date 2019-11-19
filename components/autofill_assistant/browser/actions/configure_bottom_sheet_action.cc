// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/configure_bottom_sheet_action.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/viewport_mode.h"

namespace autofill_assistant {

ConfigureBottomSheetAction::ConfigureBottomSheetAction(ActionDelegate* delegate,
                                                       const ActionProto& proto)
    : Action(delegate, proto) {}

ConfigureBottomSheetAction::~ConfigureBottomSheetAction() {}

void ConfigureBottomSheetAction::InternalProcessAction(
    ProcessActionCallback callback) {
  const ConfigureBottomSheetProto& proto = proto_.configure_bottom_sheet();

  if (proto.resize_timeout_ms() > 0) {
    // When a window resize is expected, we want to wait for the size change to
    // be visible to Javascript before moving on to another action. To do that,
    // this action registers a callback *before* making any change and waits for
    // a 'resize' event in the Javascript side.
    ViewportMode mode = delegate_->GetViewportMode();
    bool expect_resize =
        (mode != ViewportMode::RESIZE_LAYOUT_VIEWPORT &&
         proto.viewport_resizing() ==
             ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT) ||
        (mode == ViewportMode::RESIZE_LAYOUT_VIEWPORT &&
         (proto.viewport_resizing() == ConfigureBottomSheetProto::NO_RESIZE ||
          proto.viewport_resizing() ==
              ConfigureBottomSheetProto::RESIZE_VISUAL_VIEWPORT ||
          (proto.peek_mode() !=
               ConfigureBottomSheetProto::UNDEFINED_PEEK_MODE &&
           proto.peek_mode() != delegate_->GetPeekMode())));
    if (expect_resize) {
      callback_ = std::move(callback);

      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(proto.resize_timeout_ms()),
                   base::BindOnce(&ConfigureBottomSheetAction::OnTimeout,
                                  weak_ptr_factory_.GetWeakPtr()));

      delegate_->WaitForWindowHeightChange(
          base::BindOnce(&ConfigureBottomSheetAction::OnWindowHeightChange,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  switch (proto.viewport_resizing()) {
    case ConfigureBottomSheetProto::NO_CHANGE:
      break;
    case ConfigureBottomSheetProto::NO_RESIZE:
      delegate_->SetViewportMode(ViewportMode::NO_RESIZE);
      break;
    case ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT:
      delegate_->SetViewportMode(ViewportMode::RESIZE_LAYOUT_VIEWPORT);
      break;
    case ConfigureBottomSheetProto::RESIZE_VISUAL_VIEWPORT:
      delegate_->SetViewportMode(ViewportMode::RESIZE_VISUAL_VIEWPORT);
      break;
  }

  if (proto.peek_mode() != ConfigureBottomSheetProto::UNDEFINED_PEEK_MODE) {
    delegate_->SetPeekMode(proto.peek_mode());
  }

  if (callback) {
    UpdateProcessedAction(OkClientStatus());
    std::move(callback).Run(std::move(processed_action_proto_));
  }
}

void ConfigureBottomSheetAction::OnWindowHeightChange(
    const ClientStatus& status) {
  if (!callback_)
    return;

  timer_.Stop();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

void ConfigureBottomSheetAction::OnTimeout() {
  if (!callback_)
    return;

  DVLOG(2)
      << __func__
      << " Timed out waiting for window height change. Continuing anyways.";
  UpdateProcessedAction(OkClientStatus());
  processed_action_proto_->mutable_status_details()->set_original_status(
      ProcessedActionStatusProto::TIMED_OUT);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
