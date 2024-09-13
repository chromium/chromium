// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_

#include "base/feature_list.h"
#include "base/files/file_path.h"

namespace on_device_translation {

// Enables the TranslateKit Component.
BASE_DECLARE_FEATURE(kEnableTranslateKitComponent);

const char kTranslateKitRootDir[] = "translate-kit-root-dir";
const char kTranslateKitBinaryPath[] = "translate-kit-binary-path";

base::FilePath GetTranslateKitRootDirFromCommandLine();
base::FilePath GetTranslateKitBinaryPathFromCommandLine();

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_PUBLIC_CPP_FEATURES_H_
