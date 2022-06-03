// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context
//
//  The context for an audio stream used by the caption bubble. The context is
//  used for two things: for positioning the caption bubble within the context
//  widget (on Chrome browser, the browser window; on ash, the entire screen),
//  and for activating the window or tab when the Back To Tab button is clicked.
//
class CaptionBubbleContext {
 public:
  CaptionBubbleContext() = default;
  virtual ~CaptionBubbleContext() = default;
  CaptionBubbleContext(const CaptionBubbleContext&) = delete;
  CaptionBubbleContext& operator=(const CaptionBubbleContext&) = delete;

  // Returns the bounds of the context widget. On Chrome browser, this is the
  // bounds in screen of the top level widget of the browser window. When Live
  // Caption is implemented in ash, this will be bounds of the top level widget
  // of the ash window.
  virtual absl::optional<gfx::Rect> GetBounds() const = 0;

  // Activates the context. In Live Caption on browser, this activates the
  // browser window and tab of the web contents. Called when the Back To Tab
  // button is clicked in the CaptionBubble.
  virtual void Activate() = 0;

  // Whether or not the context is activatable. When Activate() is implemented
  // in child classes, the child classes must set this to be true.
  virtual bool IsActivatable() const = 0;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_H_
