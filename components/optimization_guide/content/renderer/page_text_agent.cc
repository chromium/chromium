// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/renderer/page_text_agent.h"

#include "base/functional/callback_helpers.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"

namespace optimization_guide {

namespace {

constexpr size_t kChunkSize = 4096;

std::optional<mojom::TextDumpEvent> LayoutEventAsMojoEvent(
    blink::WebMeaningfulLayout layout_event) {
  switch (layout_event) {
    case blink::WebMeaningfulLayout::kFinishedParsing:
      return mojom::TextDumpEvent::kFirstLayout;
    case blink::WebMeaningfulLayout::kFinishedLoading:
      return mojom::TextDumpEvent::kFinishedLoad;
    default:
      break;
  }
  return std::nullopt;
}

}  // namespace

PageTextAgent::PageTextAgent(content::RenderFrame* frame)
    : content::RenderFrameObserver(frame),
      content::RenderFrameObserverTracker<PageTextAgent>(frame) {
  if (!frame) {
    // For unittesting.
    return;
  }
  frame->GetAssociatedInterfaceRegistry()->AddInterface<mojom::PageTextService>(
      base::BindRepeating(&PageTextAgent::Bind, weak_factory_.GetWeakPtr()));
}
PageTextAgent::~PageTextAgent() = default;

void PageTextAgent::Bind(
    mojo::PendingAssociatedReceiver<mojom::PageTextService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

base::OnceCallback<void(scoped_refptr<const base::RefCountedString16>)>
PageTextAgent::MaybeRequestTextDumpOnLayoutEvent(
    blink::WebMeaningfulLayout event,
    uint32_t* max_size) {
  // This code path is only supported for mainframes, when there is a frame.
  if (render_frame() && !render_frame()->IsMainFrame()) {
    return base::NullCallback();
  }

  std::optional<mojom::TextDumpEvent> mojo_event =
      LayoutEventAsMojoEvent(event);
  if (!mojo_event) {
    return base::NullCallback();
  }

  auto requests_iter = requests_by_event_.find(*mojo_event);
  if (requests_iter == requests_by_event_.end()) {
    return base::NullCallback();
  }

  // Move the pending consumer remote out of the map. The map entry will be
  // destroyed since it's not needed anymore, but the pending remote will be
  // given to the callback for when the text is ready.
  mojo::PendingRemote<mojom::PageTextConsumer> pending_consumer =
      std::move(requests_iter->second.second);
  *max_size = std::max(*max_size, requests_iter->second.first->max_size);

  requests_by_event_.erase(*mojo_event);

  return base::BindOnce(&PageTextAgent::OnPageTextDump,
                        weak_factory_.GetWeakPtr(),
                        std::move(pending_consumer));
}

void PageTextAgent::OnPageTextDump(
    mojo::PendingRemote<mojom::PageTextConsumer> pending_consumer,
    scoped_refptr<const base::RefCountedString16> content) {
  mojo::Remote<mojom::PageTextConsumer> consumer;
  consumer.Bind(std::move(pending_consumer));

  if (content) {
    for (size_t i = 0; i < content->as_string().size(); i += kChunkSize) {
      // Take a substring of length |kChunkSize|, or whatever is left in
      // |content|, whichever is less.
      size_t chunk_size = std::min(kChunkSize, content->as_string().size() - i);

      // Either mojo will end up making a copy of the string (if passed a const
      // ref), or we will. Might as well just do it now to make this less
      // complex, but std::move it.
      std::u16string chunk = content->as_string().substr(i, chunk_size);
      consumer->OnTextDumpChunk(std::move(chunk));
    }
  }
  consumer->OnChunksEnd();
}

void PageTextAgent::DidFinishLoad() {
  if (!render_frame()) {
    return;
  }

  // Only subframes should use this code path.
  if (render_frame()->IsMainFrame()) {
    return;
  }

  // Only AMP subframes are supported.
  if (!is_amp_page_) {
    return;
  }

  auto requests_iter =
      requests_by_event_.find(mojom::TextDumpEvent::kFinishedLoad);
  if (requests_iter == requests_by_event_.end()) {
    return;
  }

  mojo::PendingRemote<mojom::PageTextConsumer> pending_consumer =
      std::move(requests_iter->second.second);
  uint32_t max_size = requests_iter->second.first->max_size;
  requests_by_event_.erase(mojom::TextDumpEvent::kFinishedLoad);

  auto content = base::MakeRefCounted<const base::RefCountedString16>(
      blink::WebFrameContentDumper::DumpFrameTreeAsText(
          render_frame()->GetWebFrame(), max_size)
          .Utf16());
  OnPageTextDump(std::move(pending_consumer), std::move(content));
}

void PageTextAgent::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  is_amp_page_ = false;
  // Note that |requests_by_event_| should NOT be reset here. Requests and
  // navigations from the browser race with each other, and the text dump
  // request normally wins. We don't want to drop a request when its navigation
  // is just about to start.
}

void PageTextAgent::DidObserveLoadingBehavior(
    blink::LoadingBehaviorFlag behavior) {
  is_amp_page_ |=
      behavior & blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
}

void PageTextAgent::RequestPageTextDump(
    mojom::PageTextDumpRequestPtr request,
    mojo::PendingRemote<mojom::PageTextConsumer> consumer) {
  // Save the request and consumer until the event comes along.
  mojom::TextDumpEvent event = request->event;
  requests_by_event_.emplace(
      event, std::make_pair(std::move(request), std::move(consumer)));
}

}  // namespace optimization_guide
