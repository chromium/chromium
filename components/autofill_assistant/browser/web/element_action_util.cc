// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_action_util.h"

#include "base/callback.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {
namespace element_action_util {
namespace {

void RetainElementAndExecuteCallback(
    std::unique_ptr<ElementFinderResult> element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status);
}

void PerformActionsSequentially(
    std::unique_ptr<ElementActionVector> perform_actions,
    std::unique_ptr<ProcessedActionStatusDetailsProto> status_details,
    size_t action_index,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> done,
    const ClientStatus& status) {
  status_details->MergeFrom(status.details());

  if (!status.ok()) {
    VLOG(1) << __func__ << "Web-Action failed with status " << status;
    std::move(done).Run(ClientStatus(status.proto_status(), *status_details));
    return;
  }

  if (action_index >= perform_actions->size()) {
    std::move(done).Run(ClientStatus(status.proto_status(), *status_details));
    return;
  }

  ElementActionCallback action = std::move((*perform_actions)[action_index]);
  std::move(action).Run(
      element,
      base::BindOnce(&PerformActionsSequentially, std::move(perform_actions),
                     std::move(status_details), action_index + 1, element,
                     std::move(done)));
}

}  // namespace

void PerformAll(std::unique_ptr<ElementActionVector> perform_actions,
                const ElementFinderResult& element,
                base::OnceCallback<void(const ClientStatus&)> done) {
  PerformActionsSequentially(
      std::move(perform_actions),
      std::make_unique<ProcessedActionStatusDetailsProto>(), 0, element,
      std::move(done), OkClientStatus());
}

void TakeElementAndPerform(ElementActionCallback perform,
                           base::OnceCallback<void(const ClientStatus&)> done,
                           const ClientStatus& element_status,
                           std::unique_ptr<ElementFinderResult> element) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find element.";
    std::move(done).Run(element_status);
    return;
  }

  const ElementFinderResult* element_ptr = element.get();
  std::move(perform).Run(*element_ptr,
                         base::BindOnce(&RetainElementAndExecuteCallback,
                                        std::move(element), std::move(done)));
}

}  // namespace element_action_util
}  // namespace autofill_assistant
