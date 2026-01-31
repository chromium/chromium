// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/public/paths.h"

#include "base/files/file_path.h"

namespace on_device_translation {
namespace {

constexpr base::FilePath::CharType
    kTranslateKitLanguagePackInstallationRelativeDir[] =
        FILE_PATH_LITERAL("TranslateKit/models");
constexpr base::FilePath::CharType
    kTranslateKitBinaryInstallationRelativeDir[] =
        FILE_PATH_LITERAL("TranslateKit/lib");

}  // namespace

base::FilePath GetBinaryRelativeInstallDir() {
  return base::FilePath(kTranslateKitBinaryInstallationRelativeDir);
}

base::FilePath GetLanguagePackRelativeInstallDir() {
  return base::FilePath(kTranslateKitLanguagePackInstallationRelativeDir);
}

}  // namespace on_device_translation
