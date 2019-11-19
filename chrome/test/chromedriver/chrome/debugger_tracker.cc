// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/debugger_tracker.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

DebuggerTracker::DebuggerTracker(DevToolsClient* client) {
  client->AddListener(this);
}

DebuggerTracker::~DebuggerTracker() {}

Status DebuggerTracker::OnEvent(DevToolsClient* client,
                         const std::string& method,
                         const base::DictionaryValue& params) {
  if (method == "Debugger.paused") {
    return client->SendCommandAndIgnoreResponse("Debugger.resume", {});
  }
  return Status(kOk);
}
