// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_TESTING_LANGUAGE_DETECTION_TEST_UTILS_H_
#define COMPONENTS_LANGUAGE_DETECTION_TESTING_LANGUAGE_DETECTION_TEST_UTILS_H_

#include "components/language_detection/core/language_detection_model.h"

namespace language_detection {
class LanguageDetectionModel;

// Returns a `base::File` for the valid model file.
base::File GetValidModelFile();

// Loads a valid model file from disk for testing. Will cause an expect failure
// if the model does not become available.
std::unique_ptr<LanguageDetectionModel> GetValidLanguageModel();
}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_TESTING_LANGUAGE_DETECTION_TEST_UTILS_H_
