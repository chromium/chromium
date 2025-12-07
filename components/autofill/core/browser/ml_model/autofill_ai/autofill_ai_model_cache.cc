// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"

#include <utility>

namespace autofill {

AutofillAiModelCache::FieldPrediction::FieldPrediction() = default;

AutofillAiModelCache::FieldPrediction::FieldPrediction(
    FieldType type,
    std::optional<AutofillFormatString> format_string)
    : field_type(type), format_string(std::move(format_string)) {}

AutofillAiModelCache::FieldPrediction::FieldPrediction(const FieldPrediction&) =
    default;

AutofillAiModelCache::FieldPrediction&
AutofillAiModelCache::FieldPrediction::operator=(const FieldPrediction&) =
    default;

AutofillAiModelCache::FieldPrediction::FieldPrediction(FieldPrediction&&) =
    default;

AutofillAiModelCache::FieldPrediction&
AutofillAiModelCache::FieldPrediction::operator=(FieldPrediction&&) = default;

AutofillAiModelCache::FieldPrediction::~FieldPrediction() = default;

}  // namespace autofill
