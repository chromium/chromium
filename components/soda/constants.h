// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_CONSTANTS_H_
#define COMPONENTS_SODA_CONSTANTS_H_

#include "base/files/file_path.h"

namespace speech {

enum class LanguageCode {
  kNone = 0,
  kEnUs = 1,
  kJaJp = 2,
};

// Location of the libsoda binary within the SODA installation directory.
extern const base::FilePath::CharType kSodaBinaryRelativePath[];

// Location of the SODA component relative to the components directory.
extern const base::FilePath::CharType kSodaInstallationRelativePath[];

// Location of the SODA language packs relative to the components
// directory.
extern const base::FilePath::CharType kSodaLanguagePacksRelativePath[];

// Location of the SODA en-US language pack component relative to the components
// directory.
extern const base::FilePath::CharType kSodaEnUsInstallationRelativePath[];

// Location of the SODA ja-JP language pack component relative to the components
// directory.
extern const base::FilePath::CharType kSodaJaJpInstallationRelativePath[];

// Location of the SODA models directory relative to the language pack
// installation directory.
extern const base::FilePath::CharType kSodaLanguagePackDirectoryRelativePath[];

// Get the absolute path of the SODA component directory.
const base::FilePath GetSodaDirectory();

// Get the absolute path of the SODA directory containing the language packs.
const base::FilePath GetSodaLanguagePacksDirectory();

// Get the directory containing the latest version of SODA. In most cases
// there will only be one version of SODA, but it is possible for there to be
// multiple versions if a newer version of SODA was recently downloaded before
// the old version gets cleaned up. Returns an empty path if SODA is not
// installed.
const base::FilePath GetLatestSodaDirectory();

// Get the path to the SODA binary. Returns an empty path if SODA is not
// installed.
const base::FilePath GetSodaBinaryPath();

LanguageCode GetLanguageCode(std::string language);

// Gets a collection of paths to SODA language pack directories.
std::vector<base::FilePath> GetSodaLanguagePackDirectories();

}  // namespace speech

#endif  // COMPONENTS_SODA_CONSTANTS_H_
