// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_UNIT_CONVERSION_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_UNIT_CONVERSION_CONSTANTS_H_

#include <string>

namespace quick_answers {

extern const char kRuleSetPath[];
extern const char kSourceUnitPath[];
extern const char kSourceAmountPath[];
extern const char kDestAmountPath[];
extern const char kDestTextPath[];

extern const char kCategoryPath[];
extern const char kConversionRateAPath[];
extern const char kResultValueTemplate[];
extern const char kNamePath[];
extern const char kUnitsPath[];

std::string GetUnitDisplayText(const std::string& name);

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_UNIT_CONVERSION_CONSTANTS_H_
