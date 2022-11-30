// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data_predictions.h"

namespace autofill {

FormDataPredictions::FormDataPredictions() = default;

FormDataPredictions::FormDataPredictions(const FormDataPredictions&) = default;

FormDataPredictions& FormDataPredictions::operator=(
    const FormDataPredictions&) = default;

FormDataPredictions::FormDataPredictions(FormDataPredictions&&) = default;

FormDataPredictions& FormDataPredictions::operator=(FormDataPredictions&&) =
    default;

FormDataPredictions::~FormDataPredictions() = default;

}  // namespace autofill
