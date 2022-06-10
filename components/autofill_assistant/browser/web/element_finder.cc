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
namespace {

bool HasSemanticRootFilter(const Selector& selector) {
  return selector.proto.filters_size() > 0 &&
         selector.proto.filters(0).filter_case() ==
             SelectorProto::Filter::kSemantic;
}

}  // namespace

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
  if (HasSemanticRootFilter(selector_)) {
    if (!annotate_dom_model_service_) {
      SendResult(ClientStatus(PRECONDITION_FAILED),
                 std::make_unique<ElementFinderResult>(
                     ElementFinderResult::EmptyResult()));
      return;
    }

    StartAndRetainRunner(start_element,
                         std::make_unique<SemanticElementFinder>(
                             web_contents_, devtools_client_,
                             annotate_dom_model_service_, selector_),
                         base::BindOnce(&ElementFinder::OnSemanticRunnerResult,
                                        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  StartAndRetainRunner(
      start_element,
      std::make_unique<CssElementFinder>(web_contents_, devtools_client_,
                                         user_data_, result_type_, selector_),
      base::BindOnce(&ElementFinder::SendResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ElementFinder::StartAndRetainRunner(
    const ElementFinderResult& start_element,
    std::unique_ptr<BaseElementFinder> runner,
    Callback callback) {
  runner_ = std::move(runner);
  runner_->Start(start_element, std::move(callback));
}

void ElementFinder::UpdateLogInfo(const ClientStatus& status) {
  if (!log_info_) {
    return;
  }

  auto* info = log_info_->add_element_finder_info();
  if (runner_) {
    info->MergeFrom(runner_->GetLogInfo());
  }
  info->set_status(status.proto_status());
  if (selector_.proto.has_tracking_id()) {
    info->set_tracking_id(selector_.proto.tracking_id());
  }
}

void ElementFinder::SendResult(const ClientStatus& status,
                               std::unique_ptr<ElementFinderResult> result) {
  UpdateLogInfo(status);
  DCHECK(callback_);
  std::move(callback_).Run(
      ClientStatus(status.proto_status(), status.details()), std::move(result));
}

void ElementFinder::OnSemanticRunnerResult(
    const ClientStatus& status,
    std::unique_ptr<ElementFinderResult> result) {
  if (!status.ok()) {
    SendResult(status, std::move(result));
    return;
  }

  if (selector_.proto.filters_size() > 1) {
    // The semantic filter was only the root, there are more filters to run.
    // Log and retain teh current result and start a CSS lookup from here.
    UpdateLogInfo(status);
    current_result_ = std::move(result);

    StartAndRetainRunner(
        *current_result_,
        std::make_unique<CssElementFinder>(web_contents_, devtools_client_,
                                           user_data_, result_type_, selector_),
        base::BindOnce(&ElementFinder::SendResult,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  SendResult(status, std::move(result));
}

}  // namespace autofill_assistant
