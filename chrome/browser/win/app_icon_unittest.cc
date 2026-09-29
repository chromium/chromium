// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/app_icon.h"

#include <windows.h>

#include "base/win/scoped_gdi_object.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool IsValidIcon(HICON icon) {
  ICONINFO info = {};
  if (!::GetIconInfo(icon, &info)) {
    return false;
  }

  // GetIconInfo() hands back copies of the icon's bitmaps.
  base::win::ScopedGDIObject<HBITMAP> color(info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> mask(info.hbmMask);
  return true;
}

// Callers borrow these icons rather than owning them. If either ever starts
// returning a freshly created icon, these fail, and every caller then needs to
// take ownership.
TEST(AppIconTest, AppIconIsShared) {
  const HICON icon = GetAppIcon();
  ASSERT_NE(icon, nullptr);
  EXPECT_TRUE(IsValidIcon(icon));
  EXPECT_EQ(GetAppIcon(), icon);
}

TEST(AppIconTest, SmallAppIconIsShared) {
  const HICON icon = GetSmallAppIcon();
  ASSERT_NE(icon, nullptr);
  EXPECT_TRUE(IsValidIcon(icon));
  EXPECT_EQ(GetSmallAppIcon(), icon);
}

}  // namespace
