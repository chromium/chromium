// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/unsupported_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"

namespace autofill_assistant {

UnsupportedAction::UnsupportedAction(ActionDelegate* delegate,
                                     const ActionProto& proto)
    : Action(delegate, proto) {}

UnsupportedAction::~UnsupportedAction() {}

void UnsupportedAction::InternalProcessAction(ProcessActionCallback callback) {
  UpdateProcessedAction(UNSUPPORTED_ACTION);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
