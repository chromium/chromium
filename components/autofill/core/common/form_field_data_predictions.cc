// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data_predictions.h"

namespace autofill {

FormFieldDataPredictions::FormFieldDataPredictions() = default;

FormFieldDataPredictions::FormFieldDataPredictions(
    const FormFieldDataPredictions&) = default;

FormFieldDataPredictions& FormFieldDataPredictions::operator=(
    const FormFieldDataPredictions&) = default;

FormFieldDataPredictions::FormFieldDataPredictions(FormFieldDataPredictions&&) =
    default;

FormFieldDataPredictions& FormFieldDataPredictions::operator=(
    FormFieldDataPredictions&&) = default;

FormFieldDataPredictions::~FormFieldDataPredictions() = default;

}  // namespace autofill
