// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/placeholder_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void LogPreFilledFields(const std::string_view form_type_name,
                        const std::optional<bool> initial_value_changed) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.PreFilledFields.", form_type_name}),
      initial_value_changed.has_value()
          ? AutofillPreFilledFields::kPreFilledOnPageLoad
          : AutofillPreFilledFields::kEmptyOnPageLoad);
}

}  // namespace autofill::autofill_metrics
