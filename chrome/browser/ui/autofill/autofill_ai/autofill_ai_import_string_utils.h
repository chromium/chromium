// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_STRING_UTILS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_STRING_UTILS_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

std::u16string GetPromptTitle(EntityTypeName type_name, bool is_save_prompt);

std::u16string GetPrimaryButtonText(bool is_save_prompt);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_IMPORT_STRING_UTILS_H_
