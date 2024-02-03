// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_UNIT_CONVERSION_UNITTEST_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_UNIT_CONVERSION_UNITTEST_CONSTANTS_H_

namespace quick_answers {

inline constexpr char kMassCategory[] = "Mass";

inline constexpr double kKilogramRateA = 1.0;
inline constexpr char kKilogramName[] = "Kilogram";
inline constexpr double kPoundRateA = 0.45359237;
inline constexpr double kGramRateA = 0.001;
inline constexpr char kGramName[] = "Gram";
inline constexpr double kOunceRateA = 0.028349523125;
inline constexpr char kOunceName[] = "Ounce";

inline constexpr double kSourceAmountKilogram = 100.0;
inline constexpr double kDestAmountPound = 220.462;
inline constexpr double kDestAmountGram = 100000;
inline constexpr double kDestAmountOunce = 3527.4;
inline constexpr char kSourceRawTextKilogram[] = "100 kilograms";
inline constexpr char kDestRawTextPound[] = "220.462 pounds";
inline constexpr char kDestRawTextGram[] = "100000 grams";

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_UNIT_CONVERSION_UNITTEST_CONSTANTS_H_
