// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/send_keystroke_events_action.h"

#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"

namespace autofill_assistant {

SendKeystrokeEventsAction::SendKeystrokeEventsAction(ActionDelegate* delegate,
                                                     const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_send_keystroke_events());
}

SendKeystrokeEventsAction::~SendKeystrokeEventsAction() = default;

void SendKeystrokeEventsAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  const auto& client_id =
      proto_.send_keystroke_events().client_id().identifier();
  if (client_id.empty()) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  ClientStatus status =
      delegate_->GetElementStore()->GetElement(client_id, &element_);
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  // When dealing with a password field we also want to know the last time that
  // a user has used it.
  auto selected_login_opt = delegate_->GetUserData()->selected_login_;
  auto* target_render_frame_host = element_.render_frame_host();
  if (proto_.send_keystroke_events().value().value_case() ==
          TextValue::kPasswordManagerValue &&
      selected_login_opt && target_render_frame_host) {
    // Origin check is done in PWM based on the
    // |target_render_frame_host->GetLastCommittedURL()|
    selected_login_opt->origin = target_render_frame_host->GetLastCommittedURL()
                                     .DeprecatedGetOriginAsURL();

    delegate_->GetWebsiteLoginManager()->GetGetLastTimePasswordUsed(
        *selected_login_opt,
        base::BindOnce(&SendKeystrokeEventsAction::OnGetPasswordLastTimeUsed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  ResolveTextValue();
}

void SendKeystrokeEventsAction::OnGetPasswordLastTimeUsed(
    const absl::optional<base::Time> last_time_used) {
  if (last_time_used) {
    int months_since_password_last_used =
        (base::Time::Now() - *last_time_used).InDays() / 30;
    processed_action_proto_->mutable_send_key_stroke_events_result()
        ->set_months_since_password_last_used(months_since_password_last_used);
  }

  ResolveTextValue();
}

void SendKeystrokeEventsAction::ResolveTextValue() {
  user_data::ResolveTextValue(
      proto_.send_keystroke_events().value(), element_, delegate_,
      base::BindOnce(&SendKeystrokeEventsAction::OnResolveTextValue,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SendKeystrokeEventsAction::OnResolveTextValue(const ClientStatus& status,
                                                   const std::string& value) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  delegate_->GetWebController()->SendTextInput(
      proto_.send_keystroke_events().delay_in_ms(), value, element_,
      base::BindOnce(&SendKeystrokeEventsAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SendKeystrokeEventsAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
