// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_touchable_area_action.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

SetTouchableAreaAction::SetTouchableAreaAction(ActionDelegate* delegate,
                                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_set_touchable_area());
}

SetTouchableAreaAction::~SetTouchableAreaAction() {}

void SetTouchableAreaAction::InternalProcessAction(
    ProcessActionCallback callback) {
  delegate_->SetTouchableElementArea(
      proto().set_touchable_area().element_area());
  UpdateProcessedAction(OkClientStatus());
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
