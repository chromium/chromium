// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/check_element_tag_action.h"

#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

CheckElementTagAction::CheckElementTagAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_check_element_tag());
}

CheckElementTagAction::~CheckElementTagAction() = default;

void CheckElementTagAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  ClientStatus element_status = delegate_->GetElementStore()->GetElement(
      proto_.check_element_tag().client_id().identifier(), &element_);
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  delegate_->GetWebController()->GetElementTag(
      element_, base::BindOnce(&CheckElementTagAction::OnGetElementTag,
                               weak_ptr_factory_.GetWeakPtr()));
}

void CheckElementTagAction::OnGetElementTag(const ClientStatus& status,
                                            const std::string& actual_tag) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  std::string lower_tag = base::ToLowerASCII(actual_tag);
  for (const std::string& tag : proto_.check_element_tag().any_of_tag()) {
    if (base::ToLowerASCII(tag) == lower_tag) {
      EndAction(OkClientStatus());
      return;
    }
  }

  VLOG(2) << "Expected " << proto_.check_element_tag().client_id().identifier()
          << " to have one of the following tags: "
          << base::JoinString(
                 std::vector<std::string>(
                     proto_.check_element_tag().any_of_tag().begin(),
                     proto_.check_element_tag().any_of_tag().end()),
                 ", ")
          << ", but was " << actual_tag;
  EndAction(ClientStatus(ELEMENT_MISMATCH));
}

void CheckElementTagAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
