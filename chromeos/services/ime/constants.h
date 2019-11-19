// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_CONSTANTS_H_
#define CHROMEOS_SERVICES_IME_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace chromeos {
namespace ime {

// The path where ChromeOS default input methods is installed, consisting of
// IME manifest and some bundled language dictionaries.
COMPONENT_EXPORT(CHROMEOS_IME_CONSTANTS)
extern const base::FilePath::CharType kBundledInputMethodsDirPath[];

// The path of the active user's own input methods data, including user's
// private dictionary or downloaded language dictionaries.
COMPONENT_EXPORT(CHROMEOS_IME_CONSTANTS)
extern const base::FilePath::CharType kUserInputMethodsDirPath[];

// The path of downloaded IME language dictionaries which shared by all users.
// It aims to reduce storage and improve responsiveness by reusing static
// dictionary for all users. This feature is disabled by default. When
// `CrosImeSharedDataEnabled` is on and the IME attempt to load some language
// dictionary which is missing on the device, the IME service will try to
// download it to the shared path. Because the language dictionary will be
// accessible by all users, it will prevent duplicate downloads of the same
// language dictionary.
COMPONENT_EXPORT(CHROMEOS_IME_CONSTANTS)
extern const base::FilePath::CharType kSharedInputMethodsDirPath[];

// The name of the directory inside the profile where IME data are stored in.
COMPONENT_EXPORT(CHROMEOS_IME_CONSTANTS)
extern const base::FilePath::CharType kInputMethodsDirName[];

// The name of the directory inside the input methods directory where language
// dictionaries are downloaded to.
COMPONENT_EXPORT(CHROMEOS_IME_CONSTANTS)
extern const base::FilePath::CharType kLanguageDataDirName[];

// The domain of Google Keyboard language dictionary download URL.
COMPONENT_EXPORT(CHROMEOS_IME_CONSTANTS)
extern const char kGoogleKeyboardDownloadDomain[];
}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_CONSTANTS_H_
