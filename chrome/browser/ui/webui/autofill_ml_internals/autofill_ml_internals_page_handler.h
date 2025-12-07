// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_PAGE_HANDLER_H_

#include "base/scoped_observation.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AutofillMlInternalsPageHandlerImpl
    : public autofill_ml_internals::mojom::PageHandler,
      public autofill::MlLogReceiver {
 public:
  AutofillMlInternalsPageHandlerImpl(
      mojo::PendingReceiver<autofill_ml_internals::mojom::PageHandler> receiver,
      autofill::MlLogRouter* log_router);
  AutofillMlInternalsPageHandlerImpl(
      const AutofillMlInternalsPageHandlerImpl&) = delete;
  AutofillMlInternalsPageHandlerImpl& operator=(
      const AutofillMlInternalsPageHandlerImpl&) = delete;
  ~AutofillMlInternalsPageHandlerImpl() override;

  // autofill_ml_internals::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<autofill_ml_internals::mojom::Page> page) override;

  void ProcessLog(
      const autofill_ml_internals::mojom::MlPredictionLog& log) override;

 private:
  mojo::Receiver<autofill_ml_internals::mojom::PageHandler> receiver_;
  mojo::Remote<autofill_ml_internals::mojom::Page> page_;
  base::ScopedObservation<autofill::MlLogRouter, autofill::MlLogReceiver>
      log_router_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_PAGE_HANDLER_H_
