// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/renderer/page_text_agent.h"

#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace optimization_guide {

namespace {

constexpr size_t kChunkSize = 4096;

base::Optional<mojom::TextDumpEvent> LayoutEventAsMojoEvent(
    blink::WebMeaningfulLayout layout_event) {
  switch (layout_event) {
    case blink::WebMeaningfulLayout::kFinishedParsing:
      return mojom::TextDumpEvent::kFirstLayout;
    case blink::WebMeaningfulLayout::kFinishedLoading:
      return mojom::TextDumpEvent::kFinishedLoad;
    default:
      break;
  }
  return base::nullopt;
}

}  // namespace

PageTextAgent::PageTextAgent(content::RenderFrame* frame)
    : content::RenderFrameObserver(frame),
      content::RenderFrameObserverTracker<PageTextAgent>(frame),
      frame_(frame) {
  if (!frame) {
    // For unittesting.
    return;
  }
  frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&PageTextAgent::Bind, weak_factory_.GetWeakPtr()));
}
PageTextAgent::~PageTextAgent() = default;

void PageTextAgent::Bind(
    mojo::PendingAssociatedReceiver<mojom::PageTextService> receiver) {
  receiver_.Bind(std::move(receiver));
}

void PageTextAgent::OnDestruct() {
  frame_ = nullptr;
}

bool PageTextAgent::IsInMainFrame() const {
  if (!frame_) {
    // Default to true so that |DidFinishLoad| returns early.
    return true;
  }
  return frame_->IsMainFrame();
}

uint64_t PageTextAgent::GetFrameArea() const {
  // Check that the dimensions of the frame area will never overflow the uint64.
  static_assert(
      uint64_t(std::numeric_limits<int>::max()) *
              uint64_t(std::numeric_limits<int>::max()) <=
          std::numeric_limits<uint64_t>::max(),
      "int::max() * int::max() overflows a uint64 which may cause this code to "
      "overflow");

  int width = frame_->GetWebFrame()->DocumentSize().width;
  int height = frame_->GetWebFrame()->DocumentSize().height;

  return uint64_t(width) * uint64_t(height);
}

base::OnceCallback<void(const base::string16&)>
PageTextAgent::MaybeRequestTextDumpOnLayoutEvent(
    blink::WebMeaningfulLayout event,
    uint32_t* max_size) {
  base::Optional<mojom::TextDumpEvent> mojo_event =
      LayoutEventAsMojoEvent(event);
  if (!mojo_event) {
    return base::NullCallback();
  }

  auto requests_iter = requests_by_event_.find(*mojo_event);
  if (requests_iter == requests_by_event_.end()) {
    return base::NullCallback();
  }
  const mojom::PageTextDumpRequest& request = *requests_iter->second.first;

  // Move the pending consumer remote out of the map. The map entry will be
  // destroyed since it's not needed anymore, but the pending remote will be
  // given to the callback for when the text is ready.
  mojo::PendingRemote<mojom::PageTextConsumer> pending_consumer =
      std::move(requests_iter->second.second);
  *max_size = std::max(*max_size, request.max_size);

  requests_by_event_.erase(*mojo_event);

  return base::BindOnce(&PageTextAgent::OnPageTextDump,
                        weak_factory_.GetWeakPtr(),
                        std::move(pending_consumer));
}

void PageTextAgent::DidFinishLoad() {
  DCHECK(frame_);
  if (IsInMainFrame()) {
    // Only use this event for subframes since mainframes will already get the
    // layout event call above.
    return;
  }

  auto requests_iter =
      requests_by_event_.find(mojom::TextDumpEvent::kFinishedLoad);
  if (requests_iter == requests_by_event_.end()) {
    return;
  }
  const mojom::PageTextDumpRequest& request = *requests_iter->second.first;

  if (GetFrameArea() < request.min_frame_pixel_area) {
    return;
  }

  OnPageTextDump(std::move(requests_iter->second.second),
                 blink::WebFrameContentDumper::DumpFrameTreeAsText(
                     frame_->GetWebFrame(), request.max_size)
                     .Utf16());

  requests_by_event_.erase(mojom::TextDumpEvent::kFinishedLoad);
}

void PageTextAgent::OnPageTextDump(
    mojo::PendingRemote<mojom::PageTextConsumer> pending_consumer,
    const base::string16& content) {
  mojo::Remote<mojom::PageTextConsumer> consumer;
  consumer.Bind(std::move(pending_consumer));

  for (size_t i = 0; i < content.size(); i += kChunkSize) {
    // Take a substring of length |kChunkSize|, or whatever is left in
    // |content|, whichever is less.
    size_t chunk_size = std::min(kChunkSize, content.size() - i);

    // Either mojo will end up making a copy of the string (if passed a const
    // ref), or we will. Might as well just do it now to make this less complex,
    // but std::move it.
    base::string16 chunk = content.substr(i, chunk_size);
    consumer->OnTextDumpChunk(std::move(chunk));
  }
  consumer->OnChunksEnd();
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
