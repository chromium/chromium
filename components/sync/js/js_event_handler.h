// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_JS_JS_EVENT_HANDLER_H_
#define COMPONENTS_SYNC_JS_JS_EVENT_HANDLER_H_

// See README for design comments.

#include <string>

namespace syncer {

class JsEventDetails;

// An interface for objects that handle Javascript events (e.g.,
// WebUIs).
class JsEventHandler {
 public:
  virtual void HandleJsEvent(const std::string& name,
                             const JsEventDetails& details) = 0;

 protected:
  virtual ~JsEventHandler() {}
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_JS_JS_EVENT_HANDLER_H_
