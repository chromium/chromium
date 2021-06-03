// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color_chooser.h"

#include "content/public/browser/color_chooser.h"

// The actual android color chooser is at
// components/embedder_support/android/delegate/color_chooser_android.cc

namespace chrome {

std::unique_ptr<content::ColorChooser> ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return NULL;
}

}  // namespace chrome
