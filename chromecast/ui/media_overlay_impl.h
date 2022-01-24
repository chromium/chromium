// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_MEDIA_OVERLAY_IMPL_H_
#define CHROMECAST_UI_MEDIA_OVERLAY_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/media/cma/pipeline/media_pipeline_observer.h"
#include "chromecast/ui/media_overlay.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace views {
class Label;
class ProgressBar;
class View;
class Widget;
}  // namespace views

namespace chromecast {

class CastWindowManager;

class MediaOverlayImpl : public MediaOverlay,
                         public media::MediaPipelineObserver {
 public:
  explicit MediaOverlayImpl(CastWindowManager* window_manager);
  ~MediaOverlayImpl() override;

  MediaOverlayImpl(const MediaOverlayImpl&) = delete;
  MediaOverlayImpl& operator=(const MediaOverlayImpl&) = delete;

  // MediaOverlay implementation:
  void SetController(Controller* controller) override;
  void ShowMessage(const std::u16string& message) override;
  void ShowVolumeBar(float volume) override;

  // media::MediaPipelineObserver implementation
  void OnAudioPipelineInitialized(
      media::MediaPipelineImpl* pipeline,
      const ::media::AudioDecoderConfig& config) override;
  void OnPipelineDestroyed(media::MediaPipelineImpl* pipeline) override;

 private:
  void NotifyController();

  void AddVolumeBar(views::View* container);
  void AddToast(views::View* container);

  void HideVolumeWidget();

  void ShowToast(const std::u16string& text);
  void HideToast();

  std::unique_ptr<views::Widget> CreateOverlayWidget(
      const gfx::Rect& bounds,
      std::unique_ptr<views::View> content_view);

  CastWindowManager* const window_manager_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  base::flat_set<media::MediaPipelineImpl*> passthrough_pipelines_;
  Controller* controller_;

  views::View* volume_panel_;
  views::ProgressBar* volume_bar_;
  base::OneShotTimer volume_widget_timer_;
  const gfx::Image volume_icon_image_;

  views::Label* toast_label_;
  base::OneShotTimer toast_visible_timer_;
  const gfx::FontList toast_font_list_;

  std::unique_ptr<views::Widget> overlay_widget_;

  base::WeakPtrFactory<MediaOverlayImpl> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_MEDIA_OVERLAY_IMPL_H_
