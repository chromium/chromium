// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/browser_util.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "sandbox/win/src/win_utils.h"

namespace browser_util {

bool IsBrowserAlreadyRunning() {
  static HANDLE handle = nullptr;
  base::FilePath exe_dir_path;
  // DIR_EXE is obtained from the path of FILE_EXE and, on Windows, FILE_EXE is
  // obtained from reading the PEB of the currently running process. This means
  // that even if the EXE file is moved, the DIR_EXE will still reflect the
  // original location of the EXE from when it was started. This is important as
  // IsBrowserAlreadyRunning must detect any running browser in Chrome's install
  // directory, and not in a temporary directory if it is subsequently renamed
  // or moved while running.
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir_path)) {
    // If this fails, there isn't much that can be done. However, assuming that
    // browser is *not* already running is the safer action here, as it means
    // that any pending upgrade actions will occur and hopefully the issue that
    // caused this failure will be resolved by the newer version. This might
    // cause the currently running browser to be temporarily broken, but it's
    // probably broken already if this API is failing.
    return false;
  }
  std::wstring nt_dir_name;
  if (!sandbox::GetNtPathFromWin32Path(exe_dir_path.value(), &nt_dir_name)) {
    // See above for why false is returned here.
    return false;
  }
  std::replace(nt_dir_name.begin(), nt_dir_name.end(), '\\', '!');
  std::transform(nt_dir_name.begin(), nt_dir_name.end(), nt_dir_name.begin(),
                 tolower);
  nt_dir_name = L"Global\\" + nt_dir_name;
  if (handle != NULL)
    ::CloseHandle(handle);
  handle = ::CreateEventW(NULL, TRUE, TRUE, nt_dir_name.c_str());
  int error = ::GetLastError();
  return (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED);
}

}  // namespace browser_util
