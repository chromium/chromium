// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_
#define CHROME_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_

#include "base/feature_list.h"

namespace on_device_translation {

// Enables the TranslateKit Component.
BASE_DECLARE_FEATURE(kEnableTranslateKitComponent);

const char kTranslateKitRootDir[] = "translate-kit-root-dir";
const char kTranslateKitBinaryPath[] = "translate-kit-binary-path";

}  // namespace on_device_translation

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_
