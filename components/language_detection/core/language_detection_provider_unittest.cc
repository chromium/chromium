// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/language_detection_provider.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

// Tests that we get an instance and that we can call a method.
// Since this gets a singleton that any other test could have touched already,
// we cannot expect it to be in any particular state.
TEST(LanguageDetectionProvider, Get) {
  LanguageDetectionModel& model = GetLanguageDetectionModel();
  // The most we can do is call this method to check that we have a working
  // reference.
  model.IsAvailable();
}
}  // namespace language_detection
