// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/bidi_tracker.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

BidiTracker::BidiTracker() = default;

BidiTracker::~BidiTracker() = default;

bool BidiTracker::ListensToConnections() const {
  return false;
}

Status BidiTracker::OnEvent(DevToolsClient* client,
                            const std::string& method,
                            const base::DictionaryValue& params) {
  if (method != "Runtime.bindingCalled") {
    return Status(kOk);
  }

  const base::Value* nameVal = params.FindKey("name");
  if (nameVal == nullptr) {
    return Status(kUnknownError, "Runtime.bindingCalled missing 'name'");
  }
  std::string name = nameVal->GetString();
  if (name != "sendBidiResponse") {
    // We are not interested in this function call
    return Status(kOk);
  }

  const base::Value* payloadVal = params.FindKey("payload");
  if (payloadVal == nullptr) {
    return Status(kUnknownError, "Runtime.bindingCalled missing 'payload'");
  }

  std::string payload = payloadVal->GetString();
  send_bidi_response_.Run(payload);

  return Status(kOk);
}

void BidiTracker::SetBidiCallback(SendTextFunc on_bidi_message) {
  send_bidi_response_ = std::move(on_bidi_message);
}
