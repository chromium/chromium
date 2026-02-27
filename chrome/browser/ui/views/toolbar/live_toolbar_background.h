// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_LIVE_TOOLBAR_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_LIVE_TOOLBAR_BACKGROUND_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/background.h"

class Browser;
class TabStripModel;

namespace views {
class View;
}

// A background that captures the active tab's content using
// ClientFrameSinkVideoCapturer and paints it onto the toolbar.
class LiveToolbarBackground : public views::Background,
                              public TabStripModelObserver,
                              public content::WebContentsObserver {
 public:
  explicit LiveToolbarBackground(Browser* browser);
  ~LiveToolbarBackground() override;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  void SetView(views::View* view);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // Starts capturing using ClientFrameSinkVideoCapturer.
  void StartVideoCapture();
  void StopVideoCapture();

 private:
  class VideoConsumer : public viz::mojom::FrameSinkVideoConsumer {
   public:
    explicit VideoConsumer(LiveToolbarBackground* background);
    ~VideoConsumer() override;

    // viz::mojom::FrameSinkVideoConsumer:
    void OnFrameCaptured(
        media::mojom::VideoBufferHandlePtr data,
        media::mojom::VideoFrameInfoPtr info,
        const gfx::Rect& content_rect,
        mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
            callbacks) override;
    void OnNewCaptureVersion(
        const media::CaptureVersion& capture_version) override {}
    void OnFrameWithEmptyRegionCapture() override {}
    void OnStopped() override {}
    void OnLog(const std::string& message) override {}

   private:
    raw_ptr<LiveToolbarBackground> background_;
  };

  void RetryStartCapture();
  void OnFrameCaptured(SkBitmap bitmap);

  raw_ptr<Browser> browser_;
  raw_ptr<views::View> associated_view_ = nullptr;
  SkBitmap current_frame_;

  // For VideoCapturer
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;
  std::unique_ptr<VideoConsumer> video_consumer_;
  bool is_capturing_video_ = false;

  base::RepeatingTimer retry_timer_;

  base::WeakPtrFactory<LiveToolbarBackground> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_LIVE_TOOLBAR_BACKGROUND_H_
