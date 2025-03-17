// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"

#include <string>

namespace autofill_ai {

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(std::u16string attribute_name,
                                 std::u16string attribute_value,
                                 EntityAttributeUpdateType update_type)
    : attribute_name(std::move(attribute_name)),
      attribute_value(std::move(attribute_value)),
      update_type(update_type) {}

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails() = default;

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(const SaveOrUpdateAutofillAiDataController::
                                     EntityAttributeUpdateDetails&) = default;

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::
    EntityAttributeUpdateDetails(
        SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&&) =
        default;

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::operator=(
    const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&) =
    default;

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::operator=(
    SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&&) =
    default;

SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails::
    ~EntityAttributeUpdateDetails() = default;

}  // namespace autofill_ai
