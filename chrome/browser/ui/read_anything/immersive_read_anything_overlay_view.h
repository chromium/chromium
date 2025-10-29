// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_IMMERSIVE_READ_ANYTHING_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_IMMERSIVE_READ_ANYTHING_OVERLAY_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// This view is an overlay that sits on top of the main web contents. It's
// used to house the UI for the Immersive Reading Mode feature, which provides
// a distraction-free reading mode.
class ImmersiveReadAnythingOverlayView : public views::View {
  METADATA_HEADER(ImmersiveReadAnythingOverlayView, views::View)

 public:
  ImmersiveReadAnythingOverlayView();
  ~ImmersiveReadAnythingOverlayView() override;

  ImmersiveReadAnythingOverlayView(const ImmersiveReadAnythingOverlayView&) =
      delete;
  ImmersiveReadAnythingOverlayView& operator=(
      const ImmersiveReadAnythingOverlayView&) = delete;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_IMMERSIVE_READ_ANYTHING_OVERLAY_VIEW_H_
