// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"

#include <string>

namespace autofill {

AutofillAiImportDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(std::u16string attribute_name,
                                 std::u16string attribute_value,
                                 EntityAttributeUpdateType update_type)
    : attribute_name(std::move(attribute_name)),
      attribute_value(std::move(attribute_value)),
      update_type(update_type) {}

AutofillAiImportDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails() = default;

AutofillAiImportDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(
        const AutofillAiImportDataController::EntityAttributeUpdateDetails&) =
        default;

AutofillAiImportDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(
        AutofillAiImportDataController::EntityAttributeUpdateDetails&&) =
        default;

AutofillAiImportDataController::EntityAttributeUpdateDetails&
AutofillAiImportDataController::EntityAttributeUpdateDetails::operator=(
    const AutofillAiImportDataController::EntityAttributeUpdateDetails&) =
    default;

AutofillAiImportDataController::EntityAttributeUpdateDetails&
AutofillAiImportDataController::EntityAttributeUpdateDetails::operator=(
    AutofillAiImportDataController::EntityAttributeUpdateDetails&&) = default;

AutofillAiImportDataController::EntityAttributeUpdateDetails::
    ~EntityAttributeUpdateDetails() = default;

}  // namespace autofill
