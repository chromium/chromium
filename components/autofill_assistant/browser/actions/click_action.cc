// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/click_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ClickAction::ClickAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_click());
}

ClickAction::~ClickAction() {}

void ClickAction::InternalProcessAction(ActionDelegate* delegate,
                                        ProcessActionCallback callback) {
  DCHECK_GT(proto_.click().element_to_click().selectors_size(), 0);
  delegate->WaitForElement(
      ExtractSelectors(proto_.click().element_to_click().selectors()),
      base::BindOnce(&ClickAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void ClickAction::OnWaitForElement(ActionDelegate* delegate,
                                   ProcessActionCallback callback,
                                   bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate->ClickElement(
      ExtractSelectors(proto_.click().element_to_click().selectors()),
      base::BindOnce(&::autofill_assistant::ClickAction::OnClick,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ClickAction::OnClick(ProcessActionCallback callback, bool status) {
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
