// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/focus_element_action.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

FocusElementAction::FocusElementAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_focus_element());
  show_overlay_ = false;
}

FocusElementAction::~FocusElementAction() {}

void FocusElementAction::InternalProcessAction(ActionDelegate* delegate,
                                               ProcessActionCallback callback) {
  const FocusElementProto& focus_element = proto_.focus_element();
  DCHECK_GT(focus_element.element().selectors_size(), 0);

  if (!focus_element.title().empty()) {
    delegate->ShowStatusMessage(focus_element.title());
  }
  delegate->WaitForElement(
      ExtractSelectors(focus_element.element().selectors()),
      base::BindOnce(&FocusElementAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void FocusElementAction::OnWaitForElement(ActionDelegate* delegate,
                                          ProcessActionCallback callback,
                                          bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate->FocusElement(
      ExtractSelectors(proto_.focus_element().element().selectors()),
      base::BindOnce(&FocusElementAction::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FocusElementAction::OnFocusElement(ProcessActionCallback callback,
                                        bool status) {
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
