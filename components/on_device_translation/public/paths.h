// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_PUBLIC_PATHS_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_PUBLIC_PATHS_H_

namespace base {
class FilePath;
}  // namespace base

namespace on_device_translation {

// The installation location of the TranslateKit binary component relative to
// the User Data directory.
base::FilePath GetBinaryRelativeInstallDir();

// The installation location of the TranslateKit langaage package component
// relative to the User Data directory.
base::FilePath GetLanguagePackRelativeInstallDir();

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_PUBLIC_PATHS_H_
