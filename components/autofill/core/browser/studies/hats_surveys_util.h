// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STUDIES_HATS_SURVEYS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STUDIES_HATS_SURVEYS_UTIL_H_

#include <map>
#include <string>

namespace autofill {

class AutofillClient;
class FormStructure;

// Type of key-value pairs that can be sent with a HaTS survey as metadata
// (product specific data, PSD). The keys must be configured for each survey in
// the survey configuration at `//chrome/browser/ui/hats/survey_config.cc`.
using HatsSurveyStringData = std::map<std::string, std::string>;

// If the conditions of a HaTS survey are satisfied, try to trigger the survey
// about Autofill that is related to a recently submitted form. This does not
// necessarily cause a survey to be shown to the user since the surveys are
// hidden behind a probability check. This function relies on
// `AutofillField::possible_types()` being set for the fields in
// `submitted_form`.
void MaybeTriggerFormSubmissionHatsSurveys(AutofillClient& client,
                                           const FormStructure& submitted_form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STUDIES_HATS_SURVEYS_UTIL_H_
