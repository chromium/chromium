// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_info_box_action.h"

#include <utility>

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/info_box.h"

namespace autofill_assistant {

ShowInfoBoxAction::ShowInfoBoxAction(ActionDelegate* delegate,
                                     const ActionProto& proto)
    : Action(delegate, proto) {}

ShowInfoBoxAction::~ShowInfoBoxAction() {}

void ShowInfoBoxAction::InternalProcessAction(ProcessActionCallback callback) {
  if (!proto_.show_info_box().has_info_box()) {
    delegate_->ClearInfoBox();
  } else {
    delegate_->SetInfoBox(InfoBox(proto_.show_info_box()));
  }
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
