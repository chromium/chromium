// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/key_press_handler_manager.h"

namespace autofill {

KeyPressHandlerManager::KeyPressHandlerManager(Delegate* delegate)
    : delegate_(delegate) {}

KeyPressHandlerManager::~KeyPressHandlerManager() = default;

void KeyPressHandlerManager::RegisterKeyPressHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  // It would have been nice to be able to tell if two callbacks are just the
  // same function with the same bound arguments. That's not what Equals() does
  // (they have to have the same BindState), but it's the closest approximation
  // available.
  if (handler.is_null() || (handler == handler_))
    return;

  if (!handler_.is_null())
    delegate_->RemoveHandler(handler_);
  handler_ = handler;
  delegate_->AddHandler(handler_);
}

void KeyPressHandlerManager::RemoveKeyPressHandler() {
  if (handler_.is_null())
    return;

  delegate_->RemoveHandler(handler_);
  handler_.Reset();
}

}  // namespace autofill
