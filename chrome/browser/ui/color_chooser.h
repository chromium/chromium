// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHOOSER_H_
#define CHROME_BROWSER_UI_COLOR_CHOOSER_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"

namespace content {
class ColorChooser;
class WebContents;
}  // namespace content

namespace chrome {
// Shows a color chooser that reports to the given WebContents.
std::unique_ptr<content::ColorChooser> ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color);
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COLOR_CHOOSER_H_
