// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_DOWNLOAD_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_DOWNLOAD_BUTTON_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class ImageView;
}  // namespace views

// The status of the progress ring drawn around a pinned toolbar action
// button's icon. Currently only the downloads button draws a progress ring.
// The Views toolbar paints the ring itself (see DownloadProgressRing in
// download_button_views.cc).
enum class ActionItemProgressRingStatus {
  // No ring is drawn.
  kIdle,
  // A static, solid ring is drawn. Used while the button is dormant, so that
  // the ring doesn't have to be repainted continuously in browser windows the
  // user isn't looking at.
  kDormant,
  // An indeterminate, continuously spinning ring is drawn.
  kScanning,
  // A determinate ring, filled according to the progress percentage, is drawn.
  kDownloading,
};

// The DownloadButton class is a virtual interface, defining access to the
// window's downloads toolbar button. This class exists so that the download
// UI controller can talk to the toolbar's downloads button without depending on
// the toolbar's implementation.
class DownloadButton {
 public:
  virtual ~DownloadButton() = default;

  // Returns whether the button is visible.
  virtual bool IsShowing() const = 0;

  // Returns the color the button's color provider maps `color_id` to.
  virtual SkColor GetColor(ui::ColorId color_id) const = 0;

  // Announces `text` to assistive technologies, e.g. screen readers.
  virtual void AnnounceAccessibleAlert(const std::u16string& text) = 0;

  // Updates the progress ring drawn around the button's icon.
  // `progress_percentage`, within [0, 100], is only meaningful when `status` is
  // kDownloading. Implementations pick the ring's colors based on the downloads
  // action item's enabled state and `kActionItemUnderlineIndicatorKey`
  // property, so those should be updated before calling this.
  virtual void UpdateProgressRing(ActionItemProgressRingStatus status,
                                  int progress_percentage) = 0;

  // Updates the badge showing the number of in-progress downloads. The badge
  // is only drawn when `is_active` and there are multiple in-progress
  // downloads.
  virtual void UpdateBadge(bool is_active,
                           int progress_download_count,
                           SkColor text_color,
                           SkColor background_color) = 0;

  // Returns the button's bounds in screen coordinates.
  virtual gfx::Rect GetBoundsInScreen() const = 0;

  // Sets the button's element identifier, e.g. so IPH can anchor to it.
  virtual void SetElementIdentifier(ui::ElementIdentifier element_id) = 0;

  virtual ActionItemProgressRingStatus GetProgressRingStatusForTesting() = 0;
  virtual views::ImageView* GetImageBadgeForTesting() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_DOWNLOAD_BUTTON_H_
