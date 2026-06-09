// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_INPUT_OBSERVER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_INPUT_OBSERVER_H_

#include <optional>

#include "components/cast_receiver/proto/mouse_input_service.pb.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class RenderFrameHost;
}

namespace blink {
class WebMouseEvent;
}

namespace cast_receiver {

// Observes standard Chromium input events on a WebContents (via
// RenderWidgetHost) and translates them to the input protos.
class StreamingInputObserver
    : public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver,
      public content::RenderWidgetHostObserver {
 public:
  // |web_contents| must outlive this instance.
  explicit StreamingInputObserver(content::WebContents* web_contents);
  ~StreamingInputObserver() override;

  StreamingInputObserver(const StreamingInputObserver&) = delete;
  StreamingInputObserver& operator=(const StreamingInputObserver&) = delete;

 private:
  friend class StreamingInputObserverTest;

  // content::WebContentsObserver overrides:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // content::RenderWidgetHost::InputEventObserver overrides:
  void OnInputEvent(const content::RenderWidgetHost& host,
                    const blink::WebInputEvent& event,
                    InputEventSource source) override;

  // content::RenderWidgetHostObserver overrides:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // Helper method to translate mouse events. Returns the translated proto if
  // the event should be handled, or std::nullopt if ignored.
  std::optional<cast_receiver::MouseEvent> HandleMouseEvent(
      const blink::WebMouseEvent& mouse_event,
      const gfx::Size& visible_viewport_size);

  content::RenderWidgetHost* observed_widget_ = nullptr;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_INPUT_OBSERVER_H_
