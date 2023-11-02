// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONFIRM_BUBBLE_H_
#define CHROME_BROWSER_UI_CONFIRM_BUBBLE_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

class ConfirmBubbleModel;

namespace gfx {
class Point;
}

namespace chrome {

// Creates a bubble and shows it with its top center at the specified
// |origin|. A bubble created by this function takes ownership of the
// specified |model|.
void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       std::unique_ptr<ConfirmBubbleModel> model);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_CONFIRM_BUBBLE_H_
