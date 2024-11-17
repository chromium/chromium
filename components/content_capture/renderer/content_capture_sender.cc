// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/renderer/content_capture_sender.h"

#include "base/task/single_thread_task_runner.h"
#include "components/content_capture/common/content_capture_data.h"
#include "components/content_capture/common/content_capture_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_content_holder.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content_capture {

ContentCaptureSender::ContentCaptureSender(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  registry->AddInterface<mojom::ContentCaptureSender>(base::BindRepeating(
      &ContentCaptureSender::BindPendingReceiver, base::Unretained(this)));
}

ContentCaptureSender::~ContentCaptureSender() = default;

void ContentCaptureSender::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::ContentCaptureSender>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

base::TimeDelta ContentCaptureSender::GetTaskInitialDelay() const {
  return base::Milliseconds(features::TaskInitialDelayInMilliseconds());
}

void ContentCaptureSender::DidCaptureContent(
    const blink::WebVector<blink::WebContentHolder>& data,
    bool first_data) {
  ContentCaptureData frame_data;
  FillContentCaptureData(data, &frame_data, first_data /* set_url */);
  GetContentCaptureReceiver()->DidCaptureContent(frame_data, first_data);
}

void ContentCaptureSender::DidUpdateContent(
    const blink::WebVector<blink::WebContentHolder>& data) {
  ContentCaptureData frame_data;
  FillContentCaptureData(data, &frame_data, false /* set_url */);
  GetContentCaptureReceiver()->DidUpdateContent(frame_data);
}

void ContentCaptureSender::DidRemoveContent(blink::WebVector<int64_t> data) {
  GetContentCaptureReceiver()->DidRemoveContent(data.ReleaseVector());
}

void ContentCaptureSender::StartCapture() {
  // The render_frame() is invalid after RenderFrameObserver::RenderFrameGone()
  // called.
  // Refer to crbug.com/1127082.
  if (auto* rf = render_frame()) {
    if (auto* web_frame = rf->GetWebFrame()) {
      web_frame->SetContentCaptureClient(this);
    }
  }
}

void ContentCaptureSender::StopCapture() {
  if (auto* rf = render_frame()) {
    if (auto* web_frame = rf->GetWebFrame()) {
      web_frame->SetContentCaptureClient(nullptr);
    }
  }
}

void ContentCaptureSender::OnDestruct() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void ContentCaptureSender::FillContentCaptureData(
    const blink::WebVector<blink::WebContentHolder>& node_holders,
    ContentCaptureData* data,
    bool set_url) {
  data->bounds = render_frame()->GetWebFrame()->VisibleContentRect();
  if (set_url) {
    data->value =
        render_frame()->GetWebFrame()->GetDocument().Url().GetString().Utf16();
  }
  data->children.reserve(node_holders.size());
  for (auto& holder : node_holders) {
    ContentCaptureData child;
    child.id = holder.GetId();
    child.value = holder.GetValue().Utf16();
    child.bounds = holder.GetBoundingBox();
    data->children.push_back(child);
  }
}

const mojo::AssociatedRemote<mojom::ContentCaptureReceiver>&
ContentCaptureSender::GetContentCaptureReceiver() {
  if (!content_capture_receiver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &content_capture_receiver_);
  }
  return content_capture_receiver_;
}

}  // namespace content_capture
