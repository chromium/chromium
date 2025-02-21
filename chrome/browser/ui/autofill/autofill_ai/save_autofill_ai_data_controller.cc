// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller.h"

#include <string>

namespace autofill_ai {

SaveAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(std::u16string attribute_name,
                                 std::u16string attribute_value,
                                 EntityAttributeUpdateType update_type)
    : attribute_name(std::move(attribute_name)),
      attribute_value(std::move(attribute_value)),
      update_type(update_type) {}

SaveAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails() = default;

SaveAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(
        const SaveAutofillAiDataController::EntityAttributeUpdateDetails&) =
        default;

SaveAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(
        SaveAutofillAiDataController::EntityAttributeUpdateDetails&&) = default;

SaveAutofillAiDataController::EntityAttributeUpdateDetails&
SaveAutofillAiDataController::EntityAttributeUpdateDetails::operator=(
    const SaveAutofillAiDataController::EntityAttributeUpdateDetails&) =
    default;

SaveAutofillAiDataController::EntityAttributeUpdateDetails&
SaveAutofillAiDataController::EntityAttributeUpdateDetails::operator=(
    SaveAutofillAiDataController::EntityAttributeUpdateDetails&&) = default;

SaveAutofillAiDataController::EntityAttributeUpdateDetails::
    ~EntityAttributeUpdateDetails() = default;

}  // namespace autofill_ai
