// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_CHAPTER_ITEM_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_CHAPTER_ITEM_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/cpp/chapter_information.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace global_media_controls {

// This view displays a chapter entry.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) ChapterItemView
    : public ::views::Button {
  METADATA_HEADER(ChapterItemView, ::views::Button)

 public:
  ChapterItemView(const media_session::ChapterInformation& chapter,
                  const media_message_center::MediaColorTheme& theme,
                  base::RepeatingCallback<void(const base::TimeDelta time)>
                      on_chapter_pressed);
  ChapterItemView(const ChapterItemView& other) = delete;
  ChapterItemView& operator=(const ChapterItemView& other) = delete;
  ~ChapterItemView() override;

  // Updates the image of this chapter item view.
  void UpdateArtwork(const gfx::ImageSkia& image);

  // Gets the chapter information.
  media_session::ChapterInformation chapter() { return chapter_; }

 private:
  // Jumps to the `start_time_` to play this chapter.
  void PerformAction(const ui::Event& event);

  // Sets up a custom highlight path for when the
  // `ChapterItemView` view is focused. Conditionally follows the
  // same corner rounding as the view.
  void SetUpFocusHighlight(const gfx::RoundedCornersF& item_corner_radius);

  // The passed in chapter information.
  const media_session::ChapterInformation chapter_;

  // The color theme for all the colors in this view.
  const media_message_center::MediaColorTheme theme_;

  // Runs when this view is pressed.
  base::RepeatingCallback<void(const base::TimeDelta)>
      on_chapter_pressed_callback_;

  // The `ImageView` of this chapter.
  raw_ptr<views::ImageView> artwork_view_;

  base::WeakPtrFactory<ChapterItemView> weak_ptr_factory_{this};
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_CHAPTER_ITEM_VIEW_H_
