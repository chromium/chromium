// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/js/sync_js_controller.h"

#include "base/location.h"
#include "components/sync/js/js_backend.h"
#include "components/sync/js/js_event_details.h"

namespace syncer {

SyncJsController::SyncJsController() {}

SyncJsController::~SyncJsController() {
  AttachJsBackend(WeakHandle<JsBackend>());
}

void SyncJsController::AddJsEventHandler(JsEventHandler* event_handler) {
  js_event_handlers_.AddObserver(event_handler);
  UpdateBackendEventHandler();
}

void SyncJsController::RemoveJsEventHandler(JsEventHandler* event_handler) {
  js_event_handlers_.RemoveObserver(event_handler);
  UpdateBackendEventHandler();
}

void SyncJsController::AttachJsBackend(
    const WeakHandle<JsBackend>& js_backend) {
  js_backend_ = js_backend;
  UpdateBackendEventHandler();
}

void SyncJsController::HandleJsEvent(const std::string& name,
                                     const JsEventDetails& details) {
  for (auto& observer : js_event_handlers_)
    observer.HandleJsEvent(name, details);
}

void SyncJsController::UpdateBackendEventHandler() {
  if (js_backend_.IsInitialized()) {
    // To avoid making the backend send useless events, we clear the
    // event handler we pass to it if we don't have any event
    // handlers.
    WeakHandle<JsEventHandler> backend_event_handler =
        js_event_handlers_.empty() ? WeakHandle<SyncJsController>()
                                   : MakeWeakHandle(AsWeakPtr());
    js_backend_.Call(FROM_HERE, &JsBackend::SetJsEventHandler,
                     backend_event_handler);
  }
}

}  // namespace syncer
