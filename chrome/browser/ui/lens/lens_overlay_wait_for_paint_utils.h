// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_WAIT_FOR_PAINT_UTILS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_WAIT_FOR_PAINT_UTILS_H_

#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class Browser;

namespace lens {

// Opens the given URL in the given browser and waits for the first paint to
// complete.
void WaitForPaint(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
    int browser_test_flags = ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_WAIT_FOR_PAINT_UTILS_H_
