// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_MODE_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_MODE_H_

// Type of downloads to show in the download bubble primary view.
enum class DownloadBubbleMode {
  // Shows all recent downloads finished within the last 24 hours.
  kComplete,
  // Shows only in-progress and uninteracted downloads.
  kPartial,
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_MODE_H_
