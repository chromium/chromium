// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_FIELD_PREDICTION_TEST_MATCHERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_FIELD_PREDICTION_TEST_MATCHERS_H_

#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::test {

inline ::testing::Matcher<FieldPrediction> EqualsPrediction(
    const FieldPrediction& prediction) {
  return AllOf(
      ::testing::Property("type", &FieldPrediction::type, prediction.type()),
      ::testing::Property("source", &FieldPrediction::source,
                          prediction.source()));
}

inline ::testing::Matcher<FieldPrediction> EqualsPrediction(FieldType type) {
  return ::testing::Property("type", &FieldPrediction::type, type);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
inline ::testing::Matcher<FieldPrediction> EqualsPrediction(
    FieldType type,
    FieldPrediction::Source source) {
  return EqualsPrediction(test::CreateFieldPrediction(type, source));
}
#endif

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_FIELD_PREDICTION_TEST_MATCHERS_H_
