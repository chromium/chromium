// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_NOTIFICATIONS_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_API_NOTIFICATIONS_NATIVE_HANDLER_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace base {
class Value;
}

namespace extensions {

class NotificationsNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit NotificationsNativeHandler(ScriptContext* context);

  NotificationsNativeHandler(const NotificationsNativeHandler&) = delete;
  NotificationsNativeHandler& operator=(const NotificationsNativeHandler&) =
      delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // This implements notifications_private.GetImageSizes() which
  // informs the renderer of the actual rendered size of each
  // component of a notification.  It additionally includes
  // information about the system's maximum scale factor so that
  // larger images specified in DP can be interpreted as scaled
  // versions of the DIP size.
  //   * |args| is used only to get the return value.
  //   * The return value contains the following keys:
  //         scaleFactor - a float a la devicePixelRatio
  //         icon - a dictionary with integer keys "height" and "width" (DIPs)
  //         image - a dictionary of the same format as |icon|
  //         buttonIcon - a dictionary of the same format as |icon|
  void GetNotificationImageSizes(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_NOTIFICATIONS_NATIVE_HANDLER_H_
