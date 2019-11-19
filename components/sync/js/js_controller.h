// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_JS_JS_CONTROLLER_H_
#define COMPONENTS_SYNC_JS_JS_CONTROLLER_H_

// See README for design comments.

#include <string>

namespace syncer {

class JsEventHandler;

// An interface for objects that JsEventHandlers directly interact
// with.  JsEventHandlers can add themselves to receive events and
// also send messages which will eventually reach the backend.
class JsController {
 public:
  // Adds an event handler which will start receiving JS events (not
  // immediately, so this can be called in the handler's constructor).
  // Multiple event handlers are supported, but each event handler
  // must be added at most once.
  //
  // Ideally, we'd take WeakPtrs, but we need the raw pointer values
  // to be able to look them up for removal.
  virtual void AddJsEventHandler(JsEventHandler* event_handler) = 0;

  // Removes the given event handler if it has been added.  It will
  // immediately stop receiving any JS events.
  virtual void RemoveJsEventHandler(JsEventHandler* event_handler) = 0;

 protected:
  virtual ~JsController() {}
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_JS_JS_CONTROLLER_H_
