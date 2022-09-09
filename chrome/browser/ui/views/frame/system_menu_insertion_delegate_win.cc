// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"

#include <algorithm>

size_t SystemMenuInsertionDelegateWin::GetInsertionIndex(HMENU native_menu) {
  return static_cast<size_t>(std::max(1, GetMenuItemCount(native_menu)) - 1);
}
