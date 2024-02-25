// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_UTILS_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_commands.h"
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

// This struct encapsulates a "quick action". This is displayed in the
// row view as a single icon, which triggers a download command when
// clicked.
struct DownloadBubbleQuickAction {
  DownloadCommands::Command command;
  std::u16string hover_text;
  raw_ptr<const gfx::VectorIcon> icon = nullptr;
  DownloadBubbleQuickAction(DownloadCommands::Command command,
                            const std::u16string& hover_text,
                            const gfx::VectorIcon* icon);
};

// This struct encapsulates information relevant for displaying a
// progress bar in the download bubble.
struct DownloadBubbleProgressBar {
  bool is_visible = false;
  bool is_looping = false;

  static DownloadBubbleProgressBar NoProgressBar();
  static DownloadBubbleProgressBar ProgressBar();
  static DownloadBubbleProgressBar LoopingProgressBar();
};

// Return the icon shown on both the row view and subpage.
IconAndColor IconAndColorForDownload(const DownloadUIModel& model);

std::vector<DownloadBubbleQuickAction> QuickActionsForDownload(
    const DownloadUIModel& model);

DownloadBubbleProgressBar ProgressBarForDownload(const DownloadUIModel& model);

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_UTILS_H_
