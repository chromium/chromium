// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/renderer/no_state_prefetch_utils.h"

#include "base/memory/weak_ptr.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/renderer/render_frame.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace prerender {

namespace {
const char kDeferredMediaLoadStateKey[] = "kDeferredMediaLoadStateKey";

class DeferredMediaLoadState : public base::SupportsUserData::Data {
 public:
  DeferredMediaLoadState() = default;
  ~DeferredMediaLoadState() override = default;
  DeferredMediaLoadState(const DeferredMediaLoadState&) = delete;
  DeferredMediaLoadState& operator=(const DeferredMediaLoadState&) = delete;

  static void Create(content::RenderFrame* render_frame) {
    CHECK(render_frame);
    if (!render_frame->GetUserData(kDeferredMediaLoadStateKey)) {
      render_frame->SetUserData(kDeferredMediaLoadStateKey,
                                std::make_unique<DeferredMediaLoadState>());
    }
  }

  static void Reset(content::RenderFrame* render_frame) {
    CHECK(render_frame);
    render_frame->RemoveUserData(kDeferredMediaLoadStateKey);
  }

  static bool ShouldDeferMediaLoad(content::RenderFrame* render_frame) {
    // If `render_frame` is null, defer media load as the WebFrame
    // might be gone.
    if (!render_frame) {
      return true;
    }
    return render_frame->GetUserData(kDeferredMediaLoadStateKey);
  }
};

// Defers media player loading in background pages until they're visible unless
// the tab has previously played content before.
class MediaLoadDeferrer : public blink::WebViewObserver {
 public:
  MediaLoadDeferrer(content::RenderFrame* render_frame,
                    blink::WebView* web_view,
                    base::OnceClosure continue_loading_cb)
      : blink::WebViewObserver(web_view),
        continue_loading_cb_(std::move(continue_loading_cb)) {
    mojo::PendingReceiver<media::mojom::MediaPlayerObserverClient>
        media_player_observer_client_receiver =
            media_player_observer_client_.BindNewPipeAndPassReceiver();
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        std::move(media_player_observer_client_receiver));
    media_player_observer_client_->GetHasPlayedBefore(
        base::BindOnce(&MediaLoadDeferrer::OnGetHasPlayedBeforeCallback,
                       weak_factory_.GetWeakPtr()));
  }

  MediaLoadDeferrer(const MediaLoadDeferrer&) = delete;
  MediaLoadDeferrer& operator=(const MediaLoadDeferrer&) = delete;

  ~MediaLoadDeferrer() override = default;

  // blink::WebViewObserver implementation:
  void OnDestruct() override { delete this; }
  void OnPageVisibilityChanged(
      content::PageVisibilityState visibility_state) override {
    if (visibility_state != content::PageVisibilityState::kVisible)
      return;
    std::move(continue_loading_cb_).Run();
    delete this;
  }

  void OnGetHasPlayedBeforeCallback(bool has_played_before) {
    blink::WebFrame* web_frame =
        GetWebView() ? GetWebView()->MainFrame() : nullptr;

    // If the page has played media before and doesn't require deferred
    // media load, load the player now.
    if (has_played_before && web_frame && web_frame->IsWebLocalFrame() &&
        !DeferredMediaLoadState::ShouldDeferMediaLoad(
            content::RenderFrame::FromWebFrame(web_frame->ToWebLocalFrame()))) {
      std::move(continue_loading_cb_).Run();
      delete this;
    }
  }

 private:
  mojo::Remote<media::mojom::MediaPlayerObserverClient>
      media_player_observer_client_;
  base::OnceClosure continue_loading_cb_;

  base::WeakPtrFactory<MediaLoadDeferrer> weak_factory_{this};
};

}  // namespace

bool DeferMediaLoad(content::RenderFrame* render_frame,
                    bool has_played_media_before,
                    base::OnceClosure closure) {
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  // Don't allow autoplay/autoload of media resources in a page that is hidden
  // and has no Document Picture-in-Picture window and either never played any
  // media before or the media load should be deferred in the frame. We want to
  // allow future loads even when hidden to allow playlist-like functionality.
  //
  // NOTE: This is also used to defer media loading for NoStatePrefetch.
  if ((web_frame->View()->GetVisibilityState() !=
           content::PageVisibilityState::kVisible &&
       (!has_played_media_before ||
        DeferredMediaLoadState::ShouldDeferMediaLoad(render_frame)) &&
       !web_frame->GetDocument().HasDocumentPictureInPictureWindow()) ||
      NoStatePrefetchHelper::IsPrefetching(render_frame)) {
    new MediaLoadDeferrer(render_frame, web_frame->View(), std::move(closure));
    return true;
  }

  std::move(closure).Run();
  return false;
}

void SetShouldDeferMediaLoad(content::RenderFrame* render_frame,
                             bool should_defer) {
  if (should_defer) {
    DeferredMediaLoadState::Create(render_frame);
  } else {
    DeferredMediaLoadState::Reset(render_frame);
  }
}

}  // namespace prerender
