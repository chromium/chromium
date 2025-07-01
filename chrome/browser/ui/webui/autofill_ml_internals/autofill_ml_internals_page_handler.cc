// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_ml_internals/autofill_ml_internals_page_handler.h"

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"

AutofillMlInternalsPageHandlerImpl::AutofillMlInternalsPageHandlerImpl(
    mojo::PendingReceiver<autofill_ml_internals::mojom::PageHandler> receiver,
    autofill::MLLogRouter* log_router)
    : receiver_(this, std::move(receiver)) {
  if (log_router) {
    log_router_observation_.Observe(log_router);
  }
}

void AutofillMlInternalsPageHandlerImpl::SetPage(
    mojo::PendingRemote<autofill_ml_internals::mojom::Page> page) {
  page_.Bind(std::move(page));

  // TODO: For demonstration, a dummy log is sent
  // when the page connects. This should be replaced with a proper observer
  // pattern to receive real logs.
  auto log = autofill_ml_internals::mojom::MLPredictionLog::New();
  log->form_signature = "1234567890";
  log->field_signatures = {"1111", "2222", "3333"};
  page_->OnLogAdded(std::move(log));
}

void AutofillMlInternalsPageHandlerImpl::ProcessLog(
    const autofill_ml_internals::mojom::MLPredictionLog& log) {
  page_->OnLogAdded(log.Clone());
}

AutofillMlInternalsPageHandlerImpl::~AutofillMlInternalsPageHandlerImpl() =
    default;
