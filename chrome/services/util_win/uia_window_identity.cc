// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/uia_window_identity.h"

#include <windows.h>

#include <iterator>

#include "base/containers/span.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"

bool IsChromeWindowClass(std::wstring_view window_class) {
  // Window class names are case-insensitive.
  return base::StartsWith(window_class, L"Chrome_",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsExpectedWindow(HWND hwnd,
                      const base::Process& process,
                      std::wstring_view window_class) {
  if (!hwnd || !process.IsValid() || window_class.empty() ||
      IsChromeWindowClass(window_class) || !::IsWindow(hwnd)) {
    return false;
  }

  // While `process` is held its pid is not reused, even after the process
  // exits, so a pid match means `hwnd` belongs to it. Pid() is 0 when the
  // handle lacks query access.
  DWORD owner_process_id = 0;
  if (!::GetWindowThreadProcessId(hwnd, &owner_process_id) ||
      owner_process_id != process.Pid()) {
    return false;
  }

  wchar_t actual_class[256];
  const int length =
      ::GetClassNameW(hwnd, actual_class, std::size(actual_class));
  if (length <= 0) {
    return false;
  }

  return base::EqualsCaseInsensitiveASCII(
      base::as_string_view(
          base::span(actual_class).first(static_cast<size_t>(length))),
      window_class);
}
