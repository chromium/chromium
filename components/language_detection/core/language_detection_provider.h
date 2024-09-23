// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_PROVIDER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_PROVIDER_H_

#include "base/component_export.h"
#include "components/language_detection//core/language_detection_model.h"

namespace language_detection {

COMPONENT_EXPORT(LANGUAGE_DETECTION)
BASE_DECLARE_FEATURE(kLanguageDetectionModelForTesting);

// Returns the language detection model that is shared across this process.
// TODO(https://crbug.com/354069716): The model may not have been initialized.
// Initialization is still handled by the translate component.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
LanguageDetectionModel& GetLanguageDetectionModel();

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_PROVIDER_H_
