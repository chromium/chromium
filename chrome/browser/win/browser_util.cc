// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/browser_util.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "base/logging.h"

namespace browser_util {

namespace {

// Determine the NT path name for the current process. Returns an empty path if
// a failure occurs.
std::wstring GetCurrentProcessExecutablePath() {
  std::wstring image_path;
  image_path.resize(MAX_PATH);
  DWORD path_length = image_path.size();
  BOOL success =
      ::QueryFullProcessImageNameW(::GetCurrentProcess(), PROCESS_NAME_NATIVE,
                                   image_path.data(), &path_length);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Process name is potentially greater than MAX_PATH, try larger max size.
    // https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    image_path.resize(UNICODE_STRING_MAX_CHARS);
    path_length = image_path.size();
    success =
        ::QueryFullProcessImageNameW(::GetCurrentProcess(), PROCESS_NAME_NATIVE,
                                     image_path.data(), &path_length);
  }
  if (!success) {
    PLOG_IF(ERROR, ::GetLastError() != ERROR_GEN_FAILURE)
        << "Failed to get process image path";
    return std::wstring();
  }
  image_path.resize(path_length);
  return image_path;
}

}  // namespace

bool IsBrowserAlreadyRunning() {
  static HANDLE handle = NULL;

  std::wstring nt_path_name = GetCurrentProcessExecutablePath();
  if (nt_path_name.empty()) {
    // If this fails, there isn't much that can be done. However, assuming that
    // browser is *not* already running is the safer action here, as it means
    // that any pending upgrade actions will occur and hopefully the issue that
    // caused this failure will be resolved by the newer version. This might
    // cause the currently running browser to be temporarily broken, but it's
    // probably broken already if QueryFullProcessImageNameW is failing.
    return false;
  }

  std::replace(nt_path_name.begin(), nt_path_name.end(), '\\', '!');
  std::transform(nt_path_name.begin(), nt_path_name.end(), nt_path_name.begin(),
                 tolower);
  nt_path_name = L"Global\\" + nt_path_name;
  if (handle != NULL)
    ::CloseHandle(handle);
  handle = ::CreateEventW(NULL, TRUE, TRUE, nt_path_name.c_str());
  int error = ::GetLastError();
  return (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED);
}

}  // namespace browser_util
