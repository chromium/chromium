// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/bidi_tracker.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

BidiTracker::BidiTracker() = default;

BidiTracker::~BidiTracker() = default;

bool BidiTracker::ListensToConnections() const {
  return false;
}

Status BidiTracker::OnEvent(DevToolsClient* client,
                            const std::string& method,
                            const base::Value::Dict& params) {
  if (method != "Runtime.bindingCalled") {
    return Status(kOk);
  }
  const std::string* name = params.FindString("name");
  if (!name) {
    return Status(kUnknownError, "Runtime.bindingCalled missing 'name'");
  }
  if (*name != "sendBidiResponse") {
    // We are not interested in this function call
    return Status(kOk);
  }
  const base::Value::Dict* payload = params.FindDict("payload");
  if (payload == nullptr) {
    return Status(kUnknownError, "Runtime.bindingCalled missing 'payload'");
  }
  const std::string* channel = payload->FindString("channel");
  if (!channel || channel->empty()) {
    // Internally we set non-empty channel to any BiDi command.
    // Missing or empty channel in the response means that there is a bug.
    return Status{kUnknownError, "channel is missing in the payload"};
  }
  if (!base::EndsWith(*channel, channel_suffix_)) {
    return Status{kOk};
  }
  if (send_bidi_response_.is_null()) {
    return Status{kUnknownError, "no callback is set in BidiTracker"};
  }

  return send_bidi_response_.Run(payload->Clone());
}

void BidiTracker::SetBidiCallback(SendBidiPayloadFunc on_bidi_message) {
  send_bidi_response_ = std::move(on_bidi_message);
}

const std::string& BidiTracker::ChannelSuffix() const {
  return channel_suffix_;
}

void BidiTracker::SetChannelSuffix(std::string channel_suffix) {
  channel_suffix_ = channel_suffix;
}
