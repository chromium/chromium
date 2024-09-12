// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/mock_autofill_prediction_improvements_delegate.h"

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace autofill {

MockAutofillPredictionImprovementsDelegate::
    MockAutofillPredictionImprovementsDelegate() {
  // By default make `MaybeImportForm` signal that the form was not imported by
  // user annotations so that Autofill's usual import logic will run in tests.
  ON_CALL(*this, MaybeImportForm)
      .WillByDefault([](const autofill::FormData& form,
                        const autofill::FormStructure& form_structure,
                        ImportFormCallback callback) {
        std::move(callback).Run(
            /*to_be_upserted_entries=*/{},
            /*prompt_acceptance_callback=*/base::DoNothing());
      });
}

MockAutofillPredictionImprovementsDelegate::
    ~MockAutofillPredictionImprovementsDelegate() = default;

}  // namespace autofill
