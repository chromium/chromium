// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_TEST_UTILS_H_
#define CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace blink {
class WebDocument;
}

namespace autofill {

class PasswordGenerationAgent;

// Sets that automatic generation available with |generation_agent| for fields
// |new_password_id| and |cofirm_password_id| which are in document |document|.
void SetFoundFormEligibleForGeneration(
    PasswordGenerationAgent* generation_agent,
    blink::WebDocument document,
    const char* new_password_id,
    const char* cofirm_password_id);

std::string CreateScriptToRegisterListeners(
    const char* const element_name,
    std::vector<base::string16>* variables_to_check);

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_TEST_UTILS_H_
