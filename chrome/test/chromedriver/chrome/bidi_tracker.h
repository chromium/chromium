// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_BIDI_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_BIDI_TRACKER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

namespace base {
class DictionaryValue;
}

class DevToolsClient;
class Status;
typedef base::RepeatingCallback<void(const std::string&)> SendTextFunc;

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
                 const base::DictionaryValue& params) override;

  void SetBidiCallback(SendTextFunc on_bidi_message);

 private:
  SendTextFunc send_bidi_response_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_BIDI_TRACKER_H_
