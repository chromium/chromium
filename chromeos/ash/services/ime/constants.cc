// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/constants.h"

#include <string.h>

#include "base/files/file_util.h"
#include "build/branding_buildflags.h"

#define FPL FILE_PATH_LITERAL
#define IME_DIR_STRING "input_methods"

namespace ash {
namespace ime {

const base::FilePath::CharType kInputMethodsDirName[] =
    FILE_PATH_LITERAL(IME_DIR_STRING);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const base::FilePath::CharType kBundledInputMethodsDirPath[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/input_methods/input_tools");
const base::FilePath::CharType kUserInputMethodsDirPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/" IME_DIR_STRING);
const base::FilePath::CharType kLanguageDataDirName[] =
    FILE_PATH_LITERAL("google");
#else
// IME service does not support third-party IME yet, so the paths below kind
// of act like a placeholder. In the future, put some well-designed paths here.
const base::FilePath::CharType kBundledInputMethodsDirPath[] =
    FILE_PATH_LITERAL("/tmp/" IME_DIR_STRING);
const base::FilePath::CharType kUserInputMethodsDirPath[] =
    FILE_PATH_LITERAL("/tmp/" IME_DIR_STRING);
const base::FilePath::CharType kLanguageDataDirName[] =
    FILE_PATH_LITERAL("data");
#endif

const char kGoogleKeyboardDownloadDomain[] = "dl.google.com";

}  // namespace ime
}  // namespace ash
