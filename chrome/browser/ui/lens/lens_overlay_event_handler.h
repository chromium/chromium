// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_EVENT_HANDLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_EVENT_HANDLER_H_

#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/focus/focus_manager.h"

class LensOverlayController;

namespace lens {

class LensOverlayEventHandler {
 public:
  explicit LensOverlayEventHandler(
      LensOverlayController* lens_overlay_controller);

  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event,
                           views::FocusManager* focus_manager);

 private:
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // The Lens Overlay controller to direct actions to.
  const raw_ptr<LensOverlayController> lens_overlay_controller_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_EVENT_HANDLER_H_
