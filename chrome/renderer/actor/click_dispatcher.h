// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_CLICK_DISPATCHER_H_
#define CHROME_RENDERER_ACTOR_CLICK_DISPATCHER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace actor {

// Handles the sequence of mouse move to target, mouse down, mouse up, with
// delays in between each.
//
// If cancel is called and a click has started, this will synchronously inject
// the mouse up event to ensure the page is left in a consistent state.
//
// The `on_complete` caller may safely destroy this ClickDispatcher.
class ClickDispatcher {
 public:
  // Starts the sequence of mouse events upon creation.
  //
  // The passed in `tool` must own this instance.
  //
  // `on_complete` is always called asynchronously, but isn't invoked if
  // Cancel() is invoked first.
  ClickDispatcher(blink::WebMouseEvent::Button button,
                  int count,
                  const ResolvedTarget& target,
                  const ToolBase& tool,
                  base::OnceCallback<void(mojom::ActionResultPtr)> on_complete);
  ~ClickDispatcher();

  // Cancels future dispatching, with the exception that if a mouse button is in
  // a down state, mouse up will be synchronously dispatched.
  void Cancel();

 private:
  // Clicks the mouse down, and calls DoMouseUp() asynchronously after a delay.
  void DoMouseDown(blink::WebMouseEvent::Button button,
                   int count,
                   const ResolvedTarget& target);

  // Calls DoMouseUpImpl(), then calls Finish(). Used by Cancel() so it can
  // Finish() with a different result.
  void DoMouseUp();

  // Lifts the mouse without calling Finish().
  void DoMouseUpImpl();

  // Asynchronously calls `on_complete_` with `result`. Does nothing if already
  // called, or if Cancel() has been called.
  void Finish(mojom::ActionResultPtr result);

  // The target to click on.
  const ResolvedTarget target_;

  // The tool initiating the click, containing the target.
  const raw_ref<const ToolBase> tool_;

  // Callback to invoke asynchronously after the sequence of events has
  // completed, or upon Cancel().
  base::OnceCallback<void(mojom::ActionResultPtr)> on_complete_;

  // Once the mouse has been clicked down, `mouse_up_event_` will be set so that
  // the mouse can be lifted, either after a delay, or at Cancel().
  std::optional<blink::WebMouseEvent> mouse_up_event_;

  base::WeakPtrFactory<ClickDispatcher> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_CLICK_DISPATCHER_H_
