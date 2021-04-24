// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_
#define COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_

namespace views {
class Widget;
}  // namespace views

namespace arc {

class ArcResizeLockPrefDelegate;

void ResizeToPhoneWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate);

void ResizeToTabletWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate);

void ResizeToDesktopWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate);

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_RESIZE_UTIL_H_
