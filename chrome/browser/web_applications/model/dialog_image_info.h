// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_DIALOG_IMAGE_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_DIALOG_IMAGE_INFO_H_

#include <map>

#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

// Data structure to store information about whether the icons obtained from the
// manifest need to be masked to be shown in dialogs or other UX surfaces while
// the app has not been installed yet.
struct DialogImageInfo {
  DialogImageInfo();
  ~DialogImageInfo();
  DialogImageInfo(const DialogImageInfo& dialog_image_info);
  DialogImageInfo& operator=(const DialogImageInfo& dialog_image_info);
  DialogImageInfo(DialogImageInfo&& dialog_image_info);
  DialogImageInfo& operator=(DialogImageInfo&& dialog_image_info);

  // Bitmaps keyed by their square size in px.
  std::map<int, SkBitmap> bitmaps;
  bool is_maskable = false;
};

bool operator==(const DialogImageInfo& info1, const DialogImageInfo& info2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_DIALOG_IMAGE_INFO_H_
