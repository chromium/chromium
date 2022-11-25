// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder.h"

#include <utility>

#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/base_element_finder.h"
#include "components/autofill_assistant/browser/web/css_element_finder.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

ElementFinder::ElementFinder(content::WebContents* web_contents,
                             DevtoolsClient* devtools_client,
                             const UserData* user_data,
                             ProcessedActionStatusDetailsProto* log_info,
                             const Selector& selector,
                             ElementFinderResultType result_type)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      user_data_(user_data),
      log_info_(log_info),
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

}  // namespace autofill_assistant
