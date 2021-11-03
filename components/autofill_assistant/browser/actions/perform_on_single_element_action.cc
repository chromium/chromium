// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/perform_on_single_element_action.h"

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"

namespace autofill_assistant {

PerformOnSingleElementAction::PerformOnSingleElementAction(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    bool element_is_optional,
    PerformAction perform,
    PerformTimedAction perform_timed)
    : Action(delegate, proto),
      client_id_(client_id.identifier()),
      element_is_optional_(element_is_optional),
      perform_(std::move(perform)),
      perform_timed_(std::move(perform_timed)) {}

PerformOnSingleElementAction::~PerformOnSingleElementAction() = default;

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithClientId(ActionDelegate* delegate,
                                           const ActionProto& proto,
                                           const ClientIdProto& client_id,
                                           PerformAction perform) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ false, std::move(perform),
      /* perform_timed= */ base::NullCallback()));
}

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithOptionalClientId(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    PerformAction perform) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ true, std::move(perform),
      /* perform_timed= */ base::NullCallback()));
}

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithClientIdTimed(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    PerformTimedAction perform_timed) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ false,
      /* perform= */ base::NullCallback(), std::move(perform_timed)));
}

// static
std::unique_ptr<PerformOnSingleElementAction>
PerformOnSingleElementAction::WithOptionalClientIdTimed(
    ActionDelegate* delegate,
    const ActionProto& proto,
    const ClientIdProto& client_id,
    PerformTimedAction perform_timed) {
  return base::WrapUnique(new PerformOnSingleElementAction(
      delegate, proto, client_id,
      /* element_is_optional= */ true,
      /* perform= */ base::NullCallback(), std::move(perform_timed)));
}

void PerformOnSingleElementAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (client_id_.empty() && !element_is_optional_) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  if (!client_id_.empty()) {
    ClientStatus status =
        delegate_->GetElementStore()->GetElement(client_id_, &element_);
    if (!status.ok()) {
      EndAction(status);
      return;
    }
  }

  if (proto_.send_keystroke_events().value().value_case() ==
      TextValue::kPasswordManagerValue) {
    auto login = delegate_->GetUserData()->selected_login_;

    // Origin check is done in PWM based on the
    // |target_element.container_frame_host->GetLastCommittedURL()|
    login->origin = element_.container_frame_host->GetLastCommittedURL()
                        .DeprecatedGetOriginAsURL();

    delegate_->GetWebsiteLoginManager()->GetGetLastTimePasswordUsed(
        *login, base::BindOnce(&PerformOnSingleElementAction::OnGetLastTimeUsed,
                               weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  InternalProcessActionImpl();
}

// When dealing with a password field we also want to know the last time that a
// user has used it.
void PerformOnSingleElementAction::OnGetLastTimeUsed(
    const absl::optional<base::Time> last_date_used) {
  if (last_date_used) {
    int months_since_password_last_used =
        (base::Time::Now() - last_date_used.value()).InDays() / 30;
    processed_action_proto_->mutable_send_key_stroke_events_result()
        ->set_months_since_password_last_used(months_since_password_last_used);
  }

  InternalProcessActionImpl();
}

void PerformOnSingleElementAction::InternalProcessActionImpl() {
  if (perform_) {
    std::move(perform_).Run(
        element_, base::BindOnce(&PerformOnSingleElementAction::EndAction,
                                 weak_ptr_factory_.GetWeakPtr()));
    return;
  } else if (perform_timed_) {
    std::move(perform_timed_)
        .Run(element_,
             base::BindOnce(
                 &PerformOnSingleElementAction::OnWaitForElementTimed,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::BindOnce(&PerformOnSingleElementAction::EndAction,
                                weak_ptr_factory_.GetWeakPtr())));
    return;
  }
  NOTREACHED();
  EndAction(ClientStatus(INVALID_ACTION));
}

void PerformOnSingleElementAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
