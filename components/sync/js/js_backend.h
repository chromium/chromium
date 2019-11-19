// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_JS_JS_BACKEND_H_
#define COMPONENTS_SYNC_JS_JS_BACKEND_H_

// See README for design comments.

#include <string>

namespace syncer {

class JsEventHandler;
template <typename T>
class WeakHandle;

// Interface representing the backend of chrome://sync-internals.  A
// JsBackend can handle messages and can emit events to a
// JsEventHandler.
class JsBackend {
 public:
  // Starts emitting events to the given handler, if initialized.
  virtual void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) = 0;

 protected:
  virtual ~JsBackend() {}
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_JS_JS_BACKEND_H_
