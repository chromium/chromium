// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_MEDIA_CONTROL_UI_H_
#define CHROMECAST_UI_MEDIA_CONTROL_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/ui/mojom/media_control_ui.mojom.h"
#include "chromecast/ui/vector_icons.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace chromecast {

class CastWindowManager;

// Provides a simple touch-based media UI for Aura platforms. This is used to
// enable simple touch support for media apps which are not yet touch-enabled.
// This class uses ui::views primitives to draw the UI.
class MediaControlUi : public mojom::MediaControlUi,
                       public views::ButtonListener {
 public:
  explicit MediaControlUi(CastWindowManager* window_manager);
  ~MediaControlUi() override;

  void SetBounds(const gfx::Rect& new_bounds);

  // mojom::MediaControlUi implementation:
  void SetClient(
      mojo::PendingRemote<mojom::MediaControlClient> client) override;
  void SetAttributes(mojom::MediaControlUiAttributesPtr attributes) override;

 private:
  // Only shows the media control widget if the media app window is full screen.
  void MaybeShowWidget();
  void ShowMediaControls(bool visible);
  bool visible() const;
  void OnTapped();
  std::unique_ptr<views::ImageButton> CreateImageButton(
      const gfx::VectorIcon& icon,
      int height);

  // Place elements in the locations specified by the UI spec.
  // In this case, using views::LayoutManager is more difficult since we care
  // about the position of each specific button, not the positions of view
  // children in general.
  void LayoutElements();

  // Update the media time progress bar.
  void UpdateMediaTime();

  // views::ButtonListener implementation:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  CastWindowManager* const window_manager_;
  mojo::Remote<mojom::MediaControlClient> client_;

  // This must be true to enable media overlay.
  bool app_is_fullscreen_;

  // UI components
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::View> touch_view_;
  std::unique_ptr<views::View> view_;

  // Controls
  std::unique_ptr<views::ImageButton> btn_previous_;
  std::unique_ptr<views::ImageButton> btn_play_pause_;
  std::unique_ptr<views::ImageButton> btn_next_;
  std::unique_ptr<views::ImageButton> btn_replay30_;
  std::unique_ptr<views::ImageButton> btn_forward30_;

  // Labels
  std::unique_ptr<views::Label> lbl_meta_;
  std::unique_ptr<views::Label> lbl_title_;

  // Progress
  std::unique_ptr<views::ProgressBar> progress_bar_;

  bool is_paused_;

  double media_duration_;
  // Last media playback time reported from the app.
  double last_media_time_;
  // The absolute time that |last_media_time_| was reported. This is used to
  // extrapolate the current media playback time as time progresses.
  base::TimeTicks last_media_timestamp_;

  base::RepeatingTimer media_time_update_timer_;

  views::LayoutProvider layout_provider_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MediaControlUi> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlUi);
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_MEDIA_CONTROL_UI_H_
