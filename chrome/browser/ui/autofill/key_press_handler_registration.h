// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_KEY_PRESS_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_UI_AUTOFILL_KEY_PRESS_HANDLER_REGISTRATION_H_

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_widget_host.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

// Owns the registration of a key press handler with the
// `content::RenderWidgetHost` of a `content::RenderFrameHost`.
//
// The handler is unregistered again when another frame is registered, when
// `Unregister()` is called, or when `this` is destroyed.
//
// The handler may be run at any point while the registration is alive. Callers
// whose lifetime is not tied to the lifetime of `this` must therefore guard the
// handler, e.g. by binding a `base::WeakPtr`.
class KeyPressHandlerRegistration {
 public:
  // The handler returns true if it consumed the event, i.e. if the event must
  // not be forwarded to the renderer.
  using KeyPressHandler = content::RenderWidgetHost::KeyPressEventCallback;

  KeyPressHandlerRegistration();
  ~KeyPressHandlerRegistration();

  // Registers `handler` to receive the key press events of `rfh`'s widget. A
  // prior registration, if any, is undone first.
  void Register(content::RenderFrameHost* rfh, KeyPressHandler handler);

  // Undoes the registration of `Register()`. It is safe to call this function
  // if nothing is registered or if the frame has been destroyed.
  void Unregister();

 private:
  // The frame whose key press events are handled. Empty if nothing is
  // registered.
  content::GlobalRenderFrameHostId rfh_;
  KeyPressHandler handler_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_KEY_PRESS_HANDLER_REGISTRATION_H_
