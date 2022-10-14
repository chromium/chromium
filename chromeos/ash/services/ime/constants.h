// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_CONSTANTS_H_
#define CHROMEOS_ASH_SERVICES_IME_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ash {
namespace ime {

// The path where ChromeOS default input methods is installed, consisting of
// IME manifest and some bundled language dictionaries.
COMPONENT_EXPORT(ASH_IME_CONSTANTS)
extern const base::FilePath::CharType kBundledInputMethodsDirPath[];

// The path of the active user's own input methods data, including user's
// private dictionary or downloaded language dictionaries.
COMPONENT_EXPORT(ASH_IME_CONSTANTS)
extern const base::FilePath::CharType kUserInputMethodsDirPath[];

// The name of the directory inside the profile where IME data are stored in.
COMPONENT_EXPORT(ASH_IME_CONSTANTS)
extern const base::FilePath::CharType kInputMethodsDirName[];

// The name of the directory inside the input methods directory where language
// dictionaries are downloaded to.
COMPONENT_EXPORT(ASH_IME_CONSTANTS)
extern const base::FilePath::CharType kLanguageDataDirName[];

// The domain of Google Keyboard language dictionary download URL.
COMPONENT_EXPORT(ASH_IME_CONSTANTS)
extern const char kGoogleKeyboardDownloadDomain[];

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_CONSTANTS_H_
