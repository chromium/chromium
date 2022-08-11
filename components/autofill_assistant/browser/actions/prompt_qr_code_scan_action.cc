// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_qr_code_scan_action.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

PromptQrCodeScanAction::PromptQrCodeScanAction(ActionDelegate* delegate,
                                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_prompt_qr_code_scan());
}

PromptQrCodeScanAction::~PromptQrCodeScanAction() = default;

void PromptQrCodeScanAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  if (proto_.prompt_qr_code_scan().output_client_memory_key().empty()) {
    VLOG(1) << "Invalid action: output_client_memory_key not set";
    EndAction(ClientStatus(INVALID_ACTION), /* value= */ absl::nullopt);
    return;
  }

  // Validate that all UI strings are available.
  if (proto_.prompt_qr_code_scan().use_gallery()) {
    const PromptQrCodeScanProto::ImagePickerUiStrings& image_picker_ui_strings =
        proto_.prompt_qr_code_scan().image_picker_ui_strings();
    if (image_picker_ui_strings.title_text().empty() ||
        image_picker_ui_strings.permission_text().empty() ||
        image_picker_ui_strings.permission_button_text().empty() ||
        image_picker_ui_strings.open_settings_text().empty() ||
        image_picker_ui_strings.open_settings_button_text().empty()) {
      VLOG(1) << "Invalid action: one or more image_picker_ui_strings not set";
      EndAction(ClientStatus(INVALID_ACTION), /* value= */ absl::nullopt);
      return;
    }
  } else {
    const PromptQrCodeScanProto::CameraScanUiStrings& camera_scan_ui_strings =
        proto_.prompt_qr_code_scan().camera_scan_ui_strings();
    if (camera_scan_ui_strings.title_text().empty() ||
        camera_scan_ui_strings.permission_text().empty() ||
        camera_scan_ui_strings.permission_button_text().empty() ||
        camera_scan_ui_strings.open_settings_text().empty() ||
        camera_scan_ui_strings.open_settings_button_text().empty() ||
        camera_scan_ui_strings.camera_preview_instruction_text().empty() ||
        camera_scan_ui_strings.camera_preview_security_text().empty()) {
      VLOG(1) << "Invalid action: one or more camera_scan_ui_strings not set";
      EndAction(ClientStatus(INVALID_ACTION), /* value= */ absl::nullopt);
      return;
    }
  }

  delegate_->Prompt(/* user_actions = */ nullptr,
                    /* disable_force_expand_sheet = */ false);
  delegate_->ShowQrCodeScanUi(
      std::make_unique<PromptQrCodeScanProto>(proto_.prompt_qr_code_scan()),
      base::BindOnce(&PromptQrCodeScanAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PromptQrCodeScanAction::EndAction(
    const ClientStatus& status,
    const absl::optional<ValueProto>& value) {
  if (value) {
    delegate_->GetUserModel()->SetValue(
        proto_.prompt_qr_code_scan().output_client_memory_key(), *value);
  }

  delegate_->ClearQrCodeScanUi();
  delegate_->CleanUpAfterPrompt();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
