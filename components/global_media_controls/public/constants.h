// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_CONSTANTS_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace global_media_controls {

constexpr SkColor kDefaultForegroundColor = SK_ColorBLACK;

constexpr SkColor kDefaultBackgroundColor = SK_ColorTRANSPARENT;

// The entry point through which the dialog was opened.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this in sync with its counterpart
// in tools/metrics/histograms/metadata/media/enums.xml.
enum class GlobalMediaControlsEntryPoint {
  // Through the Global Media Controls toolbar icon.
  kToolbarIcon = 0,
  // Through the use of the Presentation API (Cast) by a web page.
  kPresentation = 1,
  // Through the ChromeOS System Tray.
  kSystemTray = 2,
  // Through the mini player in the ChromeOS quick settings.
  kQuickSettingsMiniPlayer = 3,
  // Through the Cast button in the mini player in the ChromeOS quick settings.
  kQuickSettingsMiniPlayerCastButton = 4,
  kMaxValue = kQuickSettingsMiniPlayerCastButton,
};

// The minimum size in px that the media artwork can be to be displayed in the
// item.
constexpr int kMediaItemArtworkMinSize = 114;

// The desired size in px for the media artwork to be displayed in the item. The
// media session service will try and select artwork closest to this size.
constexpr int kMediaItemArtworkDesiredSize = 512;

// The preferred size of the media item updated UI in Chrome OS.
constexpr gfx::Size kCrOSMediaItemUpdatedUISize = gfx::Size(400, 150);

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_CONSTANTS_H_
