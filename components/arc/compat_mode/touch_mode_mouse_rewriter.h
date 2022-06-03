// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_TOUCH_MODE_MOUSE_REWRITER_H_
#define COMPONENTS_ARC_COMPAT_MODE_TOUCH_MODE_MOUSE_REWRITER_H_

#include "base/memory/weak_ptr.h"
#include "ui/events/event_rewriter.h"

namespace arc {

// An EventRewriter which rewrites certain mouse/trackpad events that are sent
// to phone-optimized ARC apps. For example, right click will be converted to
// long press, as in many phone-optimized apps it is normal to use long press
// for a secondary action rather than right click.
class TouchModeMouseRewriter : public ui::EventRewriter {
 public:
  TouchModeMouseRewriter();
  TouchModeMouseRewriter(const TouchModeMouseRewriter&) = delete;
  TouchModeMouseRewriter& operator=(const TouchModeMouseRewriter&) = delete;
  ~TouchModeMouseRewriter() override;

  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  void SendReleaseEvent(const ui::MouseEvent& original_event,
                        const Continuation continuation);

  bool release_event_scheduled_ = false;
  bool left_pressed_ = false;
  bool discard_next_left_release_ = false;

  base::WeakPtrFactory<TouchModeMouseRewriter> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_TOUCH_MODE_MOUSE_REWRITER_H_
