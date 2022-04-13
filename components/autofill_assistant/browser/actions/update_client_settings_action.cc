// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/update_client_settings_action.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UpdateClientSettingsAction::UpdateClientSettingsAction(ActionDelegate* delegate,
                                                       const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_update_client_settings());
}

UpdateClientSettingsAction::~UpdateClientSettingsAction() {}

void UpdateClientSettingsAction::InternalProcessAction(
    ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);
  const auto& client_settings =
      proto_.update_client_settings().client_settings();

  if (client_settings.display_strings().empty() &&
      !client_settings.display_strings_locale().empty()) {
    VLOG(1) << "Rejecting client settings update: Expected "
            << ClientSettingsProto::DisplayStringId_MAX
            << "strings, but got none";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (!client_settings.display_strings().empty()) {
    if (client_settings.display_strings_locale().empty()) {
      VLOG(1) << "Rejecting client settings update: No locale provided for "
                 "display strings.";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
    // Check if all strings are present.
    std::set<ClientSettingsProto::DisplayStringId> incoming_string_ids;
    for (const ClientSettingsProto::DisplayString& display_string :
         client_settings.display_strings()) {
      if (display_string.id() != ClientSettingsProto::UNSPECIFIED) {
        incoming_string_ids.insert(display_string.id());
      }
    }
    if (incoming_string_ids.size() < ClientSettingsProto::DisplayStringId_MAX) {
      VLOG(1) << "Rejecting client settings update: Expected "
              << ClientSettingsProto::DisplayStringId_MAX << "strings, but got "
              << incoming_string_ids.size();
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
  }
  // Note that empty display strings and empty display strings locale is ok.
  delegate_->SetClientSettings(
      proto_.update_client_settings().client_settings());
  EndAction(OkClientStatus());
}

void UpdateClientSettingsAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
