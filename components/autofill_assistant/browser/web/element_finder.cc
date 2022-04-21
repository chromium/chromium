// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder.h"

#include <utility>

#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/base_element_finder.h"
#include "components/autofill_assistant/browser/web/css_element_finder.h"
#include "components/autofill_assistant/browser/web/semantic_element_finder.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

ElementFinder::ElementFinder(
    content::WebContents* web_contents,
    DevtoolsClient* devtools_client,
    const UserData* user_data,
    ProcessedActionStatusDetailsProto* log_info,
    AnnotateDomModelService* annotate_dom_model_service,
    const Selector& selector,
    ElementFinderResultType result_type)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      user_data_(user_data),
      log_info_(log_info),
      annotate_dom_model_service_(annotate_dom_model_service),
      selector_(selector),
      result_type_(result_type) {}

ElementFinder::~ElementFinder() = default;

void ElementFinder::Start(const ElementFinderResult& start_element,
                          Callback callback) {
  callback_ = std::move(callback);

  if (selector_.empty()) {
    SendResult(ClientStatus(INVALID_SELECTOR),
               std::make_unique<ElementFinderResult>(
                   ElementFinderResult::EmptyResult()));
    return;
  }

  // TODO(b/224747076): Coordinate the dom_model_service experiment in the
  // backend. So that we don't get semantic selectors if the client doesn't
  // support the model.
  if (selector_.proto.has_semantic_information()) {
    if (!annotate_dom_model_service_) {
      SendResult(ClientStatus(PRECONDITION_FAILED),
                 std::make_unique<ElementFinderResult>(
                     ElementFinderResult::EmptyResult()));
      return;
    }

    if (selector_.proto.semantic_information().check_matches_css_element()) {
      // This will return the element being used.
      AddAndStartRunner(start_element,
                        std::make_unique<CssElementFinder>(
                            web_contents_, devtools_client_, user_data_,
                            result_type_, selector_));
    }

    AddAndStartRunner(start_element,
                      std::make_unique<SemanticElementFinder>(
                          web_contents_, devtools_client_,
                          annotate_dom_model_service_, selector_));
    return;
  }

  AddAndStartRunner(start_element, std::make_unique<CssElementFinder>(
                                       web_contents_, devtools_client_,
                                       user_data_, result_type_, selector_));
}

void ElementFinder::AddAndStartRunner(
    const ElementFinderResult& start_element,
    std::unique_ptr<BaseElementFinder> runner) {
  auto* runner_ptr = runner.get();
  runners_.emplace_back(std::move(runner));
  results_.resize(runners_.size());
  runner_ptr->Start(
      start_element,
      base::BindOnce(&ElementFinder::OnResult, weak_ptr_factory_.GetWeakPtr(),
                     /* index= */ runners_.size() - 1));
}

void ElementFinder::UpdateLogInfo(const ClientStatus& status) {
  if (log_info_ == nullptr) {
    return;
  }

  auto* info = log_info_->add_element_finder_info();
  for (const auto& runner : runners_) {
    info->MergeFrom(runner->GetLogInfo());
  }

  info->set_status(status.proto_status());
  if (selector_.proto.has_tracking_id()) {
    info->set_tracking_id(selector_.proto.tracking_id());
  }

  if (runners_.size() > 1u) {
    // By convention the 0th result is used as the result being returned for
    // usage. If there's more than one runner, use it to compare it to the
    // semantic results.
    int css_backend_node_id = runners_[0]->GetBackendNodeId();
    for (auto& predicted_element : *info->mutable_semantic_inference_result()
                                        ->mutable_predicted_elements()) {
      predicted_element.set_matches_css_element(
          predicted_element.backend_node_id() == css_backend_node_id);
    }
  }
}

void ElementFinder::SendResult(const ClientStatus& status,
                               std::unique_ptr<ElementFinderResult> result) {
  UpdateLogInfo(status);
  DCHECK(callback_);
  std::move(callback_).Run(
      ClientStatus(status.proto_status(), status.details()), std::move(result));
}

void ElementFinder::OnResult(size_t index,
                             const ClientStatus& status,
                             std::unique_ptr<ElementFinderResult> result) {
  results_[index] = std::make_pair(status, std::move(result));
  ++num_results_;

  if (num_results_ < results_.size()) {
    return;
  }

  DCHECK(!results_.empty());
  DCHECK(!runners_.empty());
  SendResult(results_[0].first, std::move(results_[0].second));
}

}  // namespace autofill_assistant
