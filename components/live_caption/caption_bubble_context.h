// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/live_caption/caption_bubble_session_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace captions {

using OpenCaptionSettingsCallback = base::RepeatingCallback<void()>;

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

  using GetBoundsCallback = base::OnceCallback<void(const gfx::Rect&)>;

  // Calls the given callback with the bounds of the context widget. On Chrome
  // browser, this is the bounds in screen of the top level widget of the
  // browser window. When Live Caption is implemented in ash, this will be
  // bounds of the top level widget of the ash window.
  //
  // If the context can't provide bounds, the callback is never executed.
  virtual void GetBounds(GetBoundsCallback callback) const = 0;

  // Returns the unique identifier for a caption bubble session. A caption
  // bubble session is per-tab and resets when a user navigates away or reloads
  // the page.
  virtual const std::string GetSessionId() const = 0;

  // Activates the context. In Live Caption on browser, this activates the
  // browser window and tab of the web contents. Called when the Back To Tab
  // button is clicked in the CaptionBubble.
  virtual void Activate() = 0;

  // Whether or not the context is activatable. When Activate() is implemented
  // in child classes, the child classes must set this to be true.
  virtual bool IsActivatable() const = 0;

  // Gets a session observer for the caption bubble context. On Chrome
  // browser, a caption bubble session is per-tab and resets when a user
  // navigates away or reloads the page.
  //
  // When this method is called, previously-created session observers are
  // invalidated (i.e. they might not execute their session-end callback) but
  // not destroyed.
  //
  // TODO(launch/4200463): Implement this for Ash if necessary.
  virtual std::unique_ptr<CaptionBubbleSessionObserver>
  GetCaptionBubbleSessionObserver() = 0;

  // Gets a callback that can be used to navigate to the caption settings page.
  // This callback is attached to the caption bubble context because
  // //components/live_caption:live_caption can't directly use the WebContents
  // to trigger a navigation due to dependency restrictions.
  virtual OpenCaptionSettingsCallback GetOpenCaptionSettingsCallback() = 0;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_H_
