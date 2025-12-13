// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_ONE_TIME_PASSWORD_SUGGESTION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_ONE_TIME_PASSWORD_SUGGESTION_DATA_H_

#include "base/types/strong_alias.h"

namespace autofill {

// A strong alias for a string corresponding to an OTP.
using OneTimePasswordSuggestionData =
    base::StrongAlias<class OneTimePasswordTag, std::string>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_ONE_TIME_PASSWORDS_ONE_TIME_PASSWORD_SUGGESTION_DATA_H_
