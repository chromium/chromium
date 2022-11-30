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
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace prerender {

namespace {

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
    render_frame->GetBrowserInterfaceBroker()->GetInterface(
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
    if (has_played_before) {
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
  // Don't allow autoplay/autoload of media resources in a page that is hidden
  // and has never played any media before.  We want to allow future loads even
  // when hidden to allow playlist-like functionality.
  //
  // NOTE: This is also used to defer media loading for NoStatePrefetch.
  if ((render_frame->GetWebFrame()->View()->GetVisibilityState() !=
           content::PageVisibilityState::kVisible &&
       !has_played_media_before) ||
      NoStatePrefetchHelper::IsPrefetching(render_frame)) {
    new MediaLoadDeferrer(render_frame, render_frame->GetWebFrame()->View(),
                          std::move(closure));
    return true;
  }

  std::move(closure).Run();
  return false;
}

}  // namespace prerender
