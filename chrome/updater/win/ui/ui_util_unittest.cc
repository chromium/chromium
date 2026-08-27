// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <windows.h>

#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace updater::ui {

TEST(UiUtilTest, IsColorDark) {
  // Pure black and white.
  EXPECT_TRUE(IsColorDark(RGB(0, 0, 0)));
  EXPECT_FALSE(IsColorDark(RGB(255, 255, 255)));

  // Midpoint gray boundaries.
  EXPECT_TRUE(IsColorDark(RGB(127, 127, 127)));
  EXPECT_FALSE(IsColorDark(RGB(128, 128, 128)));

  // Windows High Contrast Themes window background colors.
  // High Contrast Black / Night Sky (Dark)
  EXPECT_TRUE(IsColorDark(RGB(0, 0, 0)));
  // High Contrast White (Light)
  EXPECT_FALSE(IsColorDark(RGB(255, 255, 255)));
  // Aquatic theme (Dark blue/teal background)
  EXPECT_TRUE(IsColorDark(RGB(32, 32, 32)));
  EXPECT_TRUE(IsColorDark(RGB(0, 32, 48)));
  // Desert theme (Light cream/beige background)
  EXPECT_FALSE(IsColorDark(RGB(255, 250, 239)));
}

TEST(UiUtilTest, MaybeSetArrowCursor) {
  if (!base::win::IsUser32AndGdi32Available()) {
    return;
  }

  HWND parent_hwnd =
      ::CreateWindowEx(0, L"STATIC", L"Parent", WS_POPUP, 0, 0, 100, 100,
                       nullptr, nullptr, nullptr, nullptr);
  ASSERT_TRUE(parent_hwnd);
  const absl::Cleanup destroy_parent = [&] { ::DestroyWindow(parent_hwnd); };

  HWND static_child_hwnd =
      ::CreateWindowEx(0, L"STATIC", L"Child", WS_CHILD | WS_VISIBLE, 0, 0, 50,
                       50, parent_hwnd, nullptr, nullptr, nullptr);
  ASSERT_TRUE(static_child_hwnd);

  HWND edit_child_hwnd =
      ::CreateWindowEx(0, L"EDIT", L"Edit", WS_CHILD | WS_VISIBLE, 0, 50, 50,
                       50, parent_hwnd, nullptr, nullptr, nullptr);
  ASSERT_TRUE(edit_child_hwnd);

  HWND unrelated_hwnd =
      ::CreateWindowEx(0, L"STATIC", L"Unrelated", WS_POPUP, 0, 0, 100, 100,
                       nullptr, nullptr, nullptr, nullptr);
  ASSERT_TRUE(unrelated_hwnd);
  const absl::Cleanup destroy_unrelated = [&] {
    ::DestroyWindow(unrelated_hwnd);
  };

  // HTCLIENT on the window itself -> returns true.
  EXPECT_TRUE(MaybeSetArrowCursor(parent_hwnd,
                                  reinterpret_cast<WPARAM>(parent_hwnd),
                                  MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on a child window without class cursor (STATIC) -> returns true.
  EXPECT_TRUE(MaybeSetArrowCursor(parent_hwnd,
                                  reinterpret_cast<WPARAM>(static_child_hwnd),
                                  MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on a child window with its own class cursor (EDIT) -> returns
  // false.
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(edit_child_hwnd),
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on a window itself with its own non-arrow class cursor (EDIT) ->
  // returns false.
  EXPECT_FALSE(MaybeSetArrowCursor(edit_child_hwnd,
                                   reinterpret_cast<WPARAM>(edit_child_hwnd),
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // HTCLIENT on an unrelated window -> returns false.
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(unrelated_hwnd),
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));

  // Non-client hit-test (e.g. HTCAPTION) -> returns false.
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(parent_hwnd),
                                   MAKELPARAM(HTCAPTION, WM_MOUSEMOVE)));

  // Null or invalid message_wnd -> returns false.
  EXPECT_FALSE(
      MaybeSetArrowCursor(parent_hwnd, 0, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));
  EXPECT_FALSE(MaybeSetArrowCursor(parent_hwnd,
                                   reinterpret_cast<WPARAM>(parent_hwnd) + 1,
                                   MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));
}

}  // namespace updater::ui
