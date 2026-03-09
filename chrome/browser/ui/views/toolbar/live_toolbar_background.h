// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_LIVE_TOOLBAR_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_LIVE_TOOLBAR_BACKGROUND_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
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

class BrowserView;

class BrowserLiveBackgroundController : public base::SupportsUserData::Data,
                                        public TabStripModelObserver,
                                        public content::WebContentsObserver {
 public:
  explicit BrowserLiveBackgroundController(BrowserView* browser_view);
  ~BrowserLiveBackgroundController() override;

  static BrowserLiveBackgroundController* GetOrCreate(
      BrowserView* browser_view);

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnFrameCaptured() = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  SkBitmap current_frame() const { return current_frame_; }
  content::WebContents* active_web_contents() const { return web_contents(); }

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

  void StartVideoCapture();
  void StopVideoCapture();

 private:
  class VideoConsumer : public viz::mojom::FrameSinkVideoConsumer {
   public:
    explicit VideoConsumer(BrowserLiveBackgroundController* controller);
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
    raw_ptr<BrowserLiveBackgroundController> controller_;
  };

  void RetryStartCapture();
  void OnFrameCaptured(SkBitmap bitmap);

  raw_ptr<Browser> browser_;
  SkBitmap current_frame_;

  // For VideoCapturer
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;
  std::unique_ptr<VideoConsumer> video_consumer_;
  bool is_capturing_video_ = false;

  base::RepeatingTimer retry_timer_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<BrowserLiveBackgroundController> weak_factory_{this};
};

// A background that paints the live frame captured by
// BrowserLiveBackgroundController.
class LiveToolbarBackground : public views::Background,
                              public BrowserLiveBackgroundController::Observer {
 public:
  explicit LiveToolbarBackground(BrowserView* browser_view, views::View* view);
  ~LiveToolbarBackground() override;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  // BrowserLiveBackgroundController::Observer:
  void OnFrameCaptured() override;

 private:
  raw_ptr<BrowserLiveBackgroundController> controller_;
  raw_ptr<views::View> view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_LIVE_TOOLBAR_BACKGROUND_H_
