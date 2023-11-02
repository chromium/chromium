// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_UTILS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_UTILS_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace quick_answers {

const PreprocessedOutput PreprocessRequest(const IntentInfo& intent_info);

// Build title text for Quick Answers definition result.
std::string BuildDefinitionTitleText(const std::string& query_term,
                                     const std::string& phonetics);

// Build title text for Quick Answers knowledge panel entity result.
std::string BuildKpEntityTitleText(const std::string& average_score,
                                   const std::string& aggregated_count);

// Build title text for Quick Answers translation result.
std::string BuildTranslationTitleText(const IntentInfo& intent_info);

// Build title text for Quick Answers translation result.
std::string BuildTranslationTitleText(const std::string& query_text,
                                      const std::string& locale_name);

// Build display text for Quick Answers unit conversion result.
std::string BuildUnitConversionResultText(const std::string& result_value,
                                          const std::string& name);

// Unescapes the following ampersand character codes from |string|:
// &lt; &gt; &amp; &quot; &#39;
std::string UnescapeStringForHTML(const std::string& string);

// Get the ratio between the two given values (divide the larger value by the
// smaller one, so the result should be greater or equal to 1), return nullopt
// if not feasible.
absl::optional<double> GetRatio(const double value1, const double value2);

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_QUICK_ANSWERS_UTILS_H_
