// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/dispatch_js_event_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

DispatchJsEventAction::DispatchJsEventAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_dispatch_js_event());
}

DispatchJsEventAction::~DispatchJsEventAction() {}

void DispatchJsEventAction::InternalProcessAction(
    ProcessActionCallback callback) {
  delegate_->GetWebController()->DispatchJsEvent(
      base::BindOnce(&DispatchJsEventAction::OnDispatchJsEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DispatchJsEventAction::OnDispatchJsEvent(ProcessActionCallback callback,
                                              const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
