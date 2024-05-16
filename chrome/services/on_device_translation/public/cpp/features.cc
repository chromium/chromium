// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/public/cpp/features.h"

namespace on_device_translation {

BASE_FEATURE(kUseTranslateKitForTranslationAPI,
             "UseTranslatekitForTranslationAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace on_device_translation
