// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WINDOW_CONTENTS_AS_STRING_WIN_H_
#define CHROME_TEST_BASE_WINDOW_CONTENTS_AS_STRING_WIN_H_

#include <string>

#include "base/win/windows_types.h"

// Returns a string representation of the contents of |window| on the basis of
// the elements it exposes via UI automation, or an empty string in case of
// error. In particular, the control type and title of the window's element and
// each UI element within it is emitted, indented an amount corresponding to its
// depth in the UI hierarchy. Elements that are invokable (e.g., buttons) are
// labeled as such. For example:
// type: window, name: Windows can't open this type of file (.adm)
//   type: pane, name: Flyout window
//     type: pane, name:
//       type: pane, name: Immersive Openwith Flyout
//         type: text, name: Windows can't open this type of file (.adm)
//         type: pane, name:
//           type: list, name:
//             type: list, name:
//               [invokable] type: link, name: More apps
//         [invokable] type: button, name: OK
std::wstring WindowContentsAsString(HWND window_handle);

#endif  // CHROME_TEST_BASE_WINDOW_CONTENTS_AS_STRING_WIN_H_
