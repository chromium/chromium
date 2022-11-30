// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_TASKBAR_ICON_FINDER_H_
#define CHROME_BROWSER_WIN_TASKBAR_ICON_FINDER_H_

#include "base/callback.h"

namespace gfx {
class Rect;
}

// The result of a search for Chrome's taskbar icon. An empty rect is provided
// in case of error or if no icon can be found.
using TaskbarIconFinderResultCallback =
    base::OnceCallback<void(const gfx::Rect&)>;

// Asynchronosuly finds the bounding rectangle of Chrome's taskbar icon on the
// primary monitor, running |result_callback| with the result (in DIP) when
// done.
void FindTaskbarIcon(TaskbarIconFinderResultCallback result_callback);

#endif  // CHROME_BROWSER_WIN_TASKBAR_ICON_FINDER_H_
