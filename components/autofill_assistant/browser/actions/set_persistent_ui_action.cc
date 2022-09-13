// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_persistent_ui_action.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {
namespace {
bool IsCallbackAllowed(const CallbackProto::KindCase kind) {
  switch (kind) {
    case CallbackProto::kEndAction:
    case CallbackProto::kToggleUserAction:
    case CallbackProto::kSetUserActions:
    case CallbackProto::KIND_NOT_SET:
      return false;
    case CallbackProto::kSetValue:
    case CallbackProto::kShowInfoPopup:
    case CallbackProto::kShowListPopup:
    case CallbackProto::kShowCalendarPopup:
    case CallbackProto::kComputeValue:
    case CallbackProto::kSetText:
    case CallbackProto::kSetViewVisibility:
    case CallbackProto::kSetViewEnabled:
    case CallbackProto::kShowGenericPopup:
    case CallbackProto::kCreateNestedUi:
    case CallbackProto::kClearViewContainer:
    case CallbackProto::kForEach:
      return true;
      // Intentionally no default case to ensure a compilation error for new
      // cases added to the proto.
  }
}
}  // namespace

SetPersistentUiAction::SetPersistentUiAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_set_persistent_ui());
}

SetPersistentUiAction::~SetPersistentUiAction() = default;

void SetPersistentUiAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  if (!proto_.set_persistent_ui().has_generic_user_interface()) {
    VLOG(1) << "Invalid action: missing |generic user interface|";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  for (const auto& interaction : proto_.set_persistent_ui()
                                     .generic_user_interface()
                                     .interactions()
                                     .interactions()) {
    for (const auto& interaction_callback : interaction.callbacks()) {
      if (!IsCallbackAllowed(interaction_callback.kind_case())) {
        VLOG(1) << "Invalid action: interaction contains unsupported callback";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }
    }
  }

  delegate_->SetPersistentGenericUi(
      std::make_unique<GenericUserInterfaceProto>(
          proto_.set_persistent_ui().generic_user_interface()),
      base::BindOnce(&SetPersistentUiAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SetPersistentUiAction::EndAction(const ClientStatus& status) {
  if (!callback_)
    return;

  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
