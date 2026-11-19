// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TTC_POINTER_MARK_H_
#define CHROME_BROWSER_UI_VIEWS_TTC_POINTER_MARK_H_

#include <memory>

#include "ui/gfx/native_ui_types.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ttc {

// Briefly shows a ring where the get_pointer tool took the user to be
// pointing, so that they can see what the model's answer is about: around
// `center`, in screen coordinates, on top of `parent`. The ring fades out by
// itself, and takes neither input nor focus, so that it never gets between the
// user and the page. Returns the window the ring is in, which is hidden once
// the ring has faded out.
std::unique_ptr<views::Widget> ShowPointerMark(gfx::NativeView parent,
                                               const gfx::Point& center);

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_VIEWS_TTC_POINTER_MARK_H_
