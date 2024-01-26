// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_UTILS_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/color/color_id.h"

class DownloadUIModel;

namespace gfx {
struct VectorIcon;
}

// This struct encapsulates common state between the row view and
// security subpage.
struct IconAndColor {
  // This is non-null if the row should display an icon other than the system
  // icon for the filetype.
  raw_ptr<const gfx::VectorIcon> icon = nullptr;
  // kColorAlertHighSeverity, kColorAlertMediumSeverityIcon, or
  // kColorSecondaryForeground
  ui::ColorId color = ui::kColorSecondaryForeground;
};

// Return the icon shown on both the row view and subpage.
IconAndColor IconAndColorForDownload(const DownloadUIModel& model);

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_UTILS_H_
