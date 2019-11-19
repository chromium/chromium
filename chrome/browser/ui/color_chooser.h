// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHOOSER_H_
#define CHROME_BROWSER_UI_COLOR_CHOOSER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace content {
class ColorChooser;
class WebContents;
}  // namespace content

namespace chrome {
// Shows a color chooser that reports to the given WebContents.
content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                        SkColor initial_color);
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COLOR_CHOOSER_H_
