// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_LIVE_STATUS_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_LIVE_STATUS_VIEW_H_

#include "ui/color/color_id.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace global_media_controls {

// |MediaLiveStatusView| draws a straight line with a "LIVE" label in the
// middle, which replaces the progress view and shows the live status of the
// current playing media.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaLiveStatusView
    : public views::View {
  METADATA_HEADER(MediaLiveStatusView, views::View)

 public:
  explicit MediaLiveStatusView(ui::ColorId foreground_color_id,
                               ui::ColorId background_color_id);
  MediaLiveStatusView(const MediaLiveStatusView&) = delete;
  MediaLiveStatusView& operator=(const MediaLiveStatusView&) = delete;
  ~MediaLiveStatusView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // Helper functions for testing:
  views::View* GetLineViewForTesting();
  views::Label* GetLiveLabelForTesting();

 private:
  raw_ptr<views::View> line_view_ = nullptr;
  raw_ptr<views::Label> live_label_ = nullptr;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_LIVE_STATUS_VIEW_H_
