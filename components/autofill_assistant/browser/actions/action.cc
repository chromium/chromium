// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

Action::Action(const ActionProto& proto) : proto_(proto), show_overlay_(true) {}

Action::~Action() {}

void Action::ProcessAction(ActionDelegate* delegate,
                           ProcessActionCallback callback) {
  if (show_overlay_) {
    delegate->ShowOverlay();
  } else {
    delegate->HideOverlay();
  }
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  InternalProcessAction(delegate, std::move(callback));
}

void Action::UpdateProcessedAction(ProcessedActionStatusProto status) {
  // Safety check in case process action is run twice.
  *processed_action_proto_->mutable_action() = proto_;
  processed_action_proto_->set_status(status);
}

// static
std::vector<std::string> Action::ExtractSelectors(
    const google::protobuf::RepeatedPtrField<std::string>& selectors_proto) {
  std::vector<std::string> selectors;
  for (const auto& selector : selectors_proto) {
    selectors.emplace_back(selector);
  }
  return selectors;
}
}  // namespace autofill_assistant
