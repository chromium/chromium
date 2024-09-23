// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_BIDI_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_BIDI_TRACKER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

class DevToolsClient;
class Status;
using SendBidiPayloadFunc = base::RepeatingCallback<Status(base::Value::Dict)>;

// Tracks the state of the DOM and BiDi messages coming from the browser
class BidiTracker : public DevToolsEventListener {
 public:
  BidiTracker();

  BidiTracker(const BidiTracker&) = delete;
  BidiTracker& operator=(const BidiTracker&) = delete;

  ~BidiTracker() override;

  // Overridden from DevToolsEventListener:
  bool ListensToConnections() const override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

  void SetBidiCallback(SendBidiPayloadFunc on_bidi_message);

  const std::string& ChannelSuffix() const;
  void SetChannelSuffix(std::string channel_suffix);

 private:
  SendBidiPayloadFunc send_bidi_response_;
  std::string channel_suffix_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_BIDI_TRACKER_H_
