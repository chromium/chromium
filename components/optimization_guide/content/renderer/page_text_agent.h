// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_RENDERER_PAGE_TEXT_AGENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_RENDERER_PAGE_TEXT_AGENT_H_

#include <stdint.h>
#include <map>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace optimization_guide {

// PageTextAgent is the interface between ChromeRenderFrameObserver and
// mojom::PageTextService. It currently supports requesting and getting text
// dumps during |content::RenderFrameObserver::DidMeaningfulLayout|, but more
// events will be added in the future.
class PageTextAgent
    : public mojom::PageTextService,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<PageTextAgent> {
 public:
  explicit PageTextAgent(content::RenderFrame* frame);
  ~PageTextAgent() override;

  // This should be called during |DidMeaningfulLayout| to determine whether
  // this class would like to get a page dump. If so, the returned callback
  // should be ran with the text, and |max_size| will be updated to a bigger
  // value iff this class wants more text than that.
  base::OnceCallback<void(const base::string16&)>
  MaybeRequestTextDumpOnLayoutEvent(blink::WebMeaningfulLayout event,
                                    uint32_t* max_size);

  // Bind to mojo pipes. Public for testing.
  void Bind(mojo::PendingAssociatedReceiver<mojom::PageTextService> receiver);

  // mojom::PageTextService:
  void RequestPageTextDump(
      mojom::PageTextDumpRequestPtr request,
      mojo::PendingRemote<mojom::PageTextConsumer> consumer) override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidFinishLoad() override;

  PageTextAgent(const PageTextAgent&) = delete;
  PageTextAgent& operator=(const PageTextAgent&) = delete;

 protected:
  // Virtual for testing.
  virtual uint64_t GetFrameArea() const;
  virtual bool IsInMainFrame() const;

 private:
  // Called when the text dump is done and it can be sent to |consumer|.
  void OnPageTextDump(mojo::PendingRemote<mojom::PageTextConsumer> consumer,
                      const base::string16& content);

  content::RenderFrame* frame_;

  // Keeps track of the text dump events that have been requested. Entries are
  // only present between being added in |RequestPageTextDump| and
  // |MaybeRequestTextDumpOnLayoutEvent| where relevant data to make the
  // response is moved into the callback.
  using RequestAndConsumer =
      std::pair<mojom::PageTextDumpRequestPtr,
                mojo::PendingRemote<mojom::PageTextConsumer>>;
  std::map<mojom::TextDumpEvent, RequestAndConsumer> requests_by_event_;

  mojo::AssociatedReceiver<mojom::PageTextService> receiver_{this};

  base::WeakPtrFactory<PageTextAgent> weak_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_RENDERER_PAGE_TEXT_AGENT_H_
