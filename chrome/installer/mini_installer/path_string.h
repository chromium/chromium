// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_PATH_STRING_H_
#define CHROME_INSTALLER_MINI_INSTALLER_PATH_STRING_H_

#include <windows.h>

#include "chrome/installer/mini_installer/mini_string.h"

namespace mini_installer {

// A string sufficiently large to hold a Windows file path. Note that starting
// with Windows 10 1607, applications may opt-in to supporting long paths via
// the longPathAware element in the app's manifest. Chrome does not do this. See
// https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation
// for details.
using PathString = StackString<MAX_PATH>;

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_PATH_STRING_H_
