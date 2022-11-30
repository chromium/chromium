// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_HOST_HELPER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_HOST_HELPER_H_

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/geometry/point.h"

@class NSView;

namespace ui {
class TextInputClient;
}  // namespace ui

namespace remote_cocoa {

class DragDropClient;

// This is a helper class for the mojo interface NativeWidgetNSWindowHost.
// This provides an easier-to-use interface than the mojo for selected
// functions. It also is temporarily exposing functionality that is not yet
// implemented over mojo.
class REMOTE_COCOA_APP_SHIM_EXPORT NativeWidgetNSWindowHostHelper {
 public:
  virtual ~NativeWidgetNSWindowHostHelper() = default;

  // Retrieve the NSObject for accessibility for this widget.
  virtual id GetNativeViewAccessible() = 0;

  // Synchronously dispatch a key event. Note that this function will modify
  // |event| based on whether or not it was handled.
  virtual void DispatchKeyEvent(ui::KeyEvent* event) = 0;

  // Synchronously dispatch a key event to the current menu controller (if one
  // exists and it is owned by the widget for this). Return true if the event
  // was swallowed (that is, if the menu's dispatch returned
  // POST_DISPATCH_NONE). Note that this function will modify |event| based on
  // whether or not it was handled.
  virtual bool DispatchKeyEventToMenuController(ui::KeyEvent* event) = 0;

  // Synchronously query the quicklook text at |location_in_content|. Return in
  // |found_word| whether or not a word was found.
  // TODO(ccameron): This needs gfx::DecoratedText to be mojo-ified before it
  // can be done over mojo.
  virtual void GetWordAt(const gfx::Point& location_in_content,
                         bool* found_word,
                         gfx::DecoratedText* decorated_word,
                         gfx::Point* baseline_point) = 0;

  // Return a pointer to host's DragDropClientMac.
  // TODO(ccameron): Drag-drop behavior needs to be implemented over mojo.
  virtual DragDropClient* GetDragDropClient() = 0;

  // Return a pointer to the host's ui::TextInputClient.
  // TODO(ccameron): Remove the needs for this call.
  virtual ui::TextInputClient* GetTextInputClient() = 0;

  // Return whether or not the nested run loop to animate in a sheet must be
  // made on a fresh stack, produced by posting a task to make the call.
  // https://crbug.com/1234509
  virtual bool MustPostTaskToRunModalSheetAnimation() const = 0;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_HOST_HELPER_H_
