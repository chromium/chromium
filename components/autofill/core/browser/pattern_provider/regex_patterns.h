// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_REGEX_PATTERNS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_REGEX_PATTERNS_H_

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

namespace autofill {

std::map<std::string, std::map<LanguageCode, std::vector<MatchingPattern>>>
CreateRegexPatterns();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_REGEX_PATTERNS_H_
