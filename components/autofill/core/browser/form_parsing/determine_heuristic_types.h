// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_DETERMINE_HEURISTIC_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_DETERMINE_HEURISTIC_TYPES_H_

#include "base/containers/span.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class FormStructure;
class LogManager;

// Runs several heuristics against the form fields to determine their possible
// types.
void DetermineHeuristicTypes(const GeoIpCountryCode& client_country,
                             const LanguageCode& current_page_language,
                             FormStructure& form,
                             LogManager* log_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_DETERMINE_HEURISTIC_TYPES_H_
