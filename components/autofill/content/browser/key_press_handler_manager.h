// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_KEY_PRESS_HANDLER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_KEY_PRESS_HANDLER_MANAGER_H_

#include "base/macros.h"
#include "content/public/browser/render_widget_host.h"

namespace autofill {

// KeyPressHandlerManager allows registering a key press handler and ensuring
// its unregistering in case of destruction of the manager or request for
// registration of another handler. It still needs a Delegate implementation to
// use the low-level handler registration API.

class KeyPressHandlerManager {
 public:
  class Delegate {
   public:
    virtual void AddHandler(
        const content::RenderWidgetHost::KeyPressEventCallback& handler) = 0;
    virtual void RemoveHandler(
        const content::RenderWidgetHost::KeyPressEventCallback& handler) = 0;
  };

  explicit KeyPressHandlerManager(Delegate* delegate);

  virtual ~KeyPressHandlerManager();

  void RegisterKeyPressHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler);

  void RemoveKeyPressHandler();  // Unregisters previous handler.

 private:
  Delegate* const delegate_;

  content::RenderWidgetHost::KeyPressEventCallback handler_;

  DISALLOW_COPY_AND_ASSIGN(KeyPressHandlerManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_KEY_PRESS_HANDLER_MANAGER_H_
