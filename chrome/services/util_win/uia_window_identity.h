// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_UIA_WINDOW_IDENTITY_H_
#define CHROME_SERVICES_UTIL_WIN_UIA_WINDOW_IDENTITY_H_

#include <string_view>

#include "base/win/windows_types.h"

namespace base {
class Process;
}

// Returns true for the window classes Chrome-based apps use, in any case:
// Chrome_WidgetWin_*, Chrome_RenderWidgetHostHWND, Chrome_MessageWindow.
bool IsChromeWindowClass(std::wstring_view window_class);

// Returns true if `hwnd` is alive, is owned by `process`, and has class
// `window_class`, compared case-insensitively. Always false for Chrome's
// classes. `process` needs PROCESS_QUERY_LIMITED_INFORMATION.
bool IsExpectedWindow(HWND hwnd,
                      const base::Process& process,
                      std::wstring_view window_class);

#endif  // CHROME_SERVICES_UTIL_WIN_UIA_WINDOW_IDENTITY_H_
