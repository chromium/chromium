// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_OBSERVER_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_OBSERVER_H_

#include <string>

#include "chromecast/browser/mojom/cast_web_contents.mojom.h"
#include "chromecast/browser/web_types.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace chromecast {

class CastWebContents;

// Default implementation of mojom::CastWebContentsObserver.
class CastWebContentsObserver : public mojom::CastWebContentsObserver {
 public:
  CastWebContentsObserver();

  // Adds |this| to the CastWebContents observer list. Observe(nullptr) will
  // remove |this| from the observer list of the current CastWebContents being
  // observed.
  void Observe(mojom::CastWebContents* cast_web_contents);

  // =========================================================================
  // Observer Methods
  // =========================================================================

  // Advertises page state for the CastWebContents.
  void PageStateChanged(PageState page_state) override {}

  // Called when the page has stopped. e.g.: A 404 occurred when loading the
  // page or if the render process for the main frame crashes. |error_code|
  // will return a net::Error describing the failure, or net::OK if the page
  // closed intentionally.
  //
  // After this method, the page state will be one of the following:
  // CLOSED: Page was closed as expected and the WebContents exists. The page
  //     should generally not be reloaded, since the page closure was
  //     triggered intentionally.
  // ERROR: Page is in an error state. It should be reloaded or deleted.
  // DESTROYED: Page was closed due to deletion of WebContents. The
  //     CastWebContents instance is no longer usable and should be deleted.
  void PageStopped(PageState page_state, int error_code) override {}

  // A new RenderFrame was created for the WebContents. |settings_manager| is
  // provided by the frame.
  void RenderFrameCreated(int render_process_id, int render_frame_id) override {
  }

  // A navigation has finished in the WebContents' main frame.
  void MainFrameFinishedNavigation() override {}

  // These methods are calls forwarded from WebContentsObserver.
  void UpdateTitle(const std::string& title) override {}
  void UpdateFaviconURL(const GURL& url) override {}
  void DidFirstVisuallyNonEmptyPaint() override {}

  // Notifies that a resource for the main frame failed to load.
  void ResourceLoadFailed() override {}

  // Propagates the process information via observer, in particular to
  // the underlying OnRendererProcessStarted() method.
  void OnRenderProcessReady(int pid) override {}

  // Notify media playback state changes for the underlying WebContents.
  void MediaPlaybackChanged(bool media_playing) override {}

  void InnerContentsCreated(mojo::PendingRemote<mojom::CastWebContents>
                                pending_inner_contents) override {}

 protected:
  ~CastWebContentsObserver() override;

  mojo::Receiver<mojom::CastWebContentsObserver> receiver_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_OBSERVER_H_
