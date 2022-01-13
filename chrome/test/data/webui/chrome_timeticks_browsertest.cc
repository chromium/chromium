// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/chrome_timeticks_browsertest.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::WebUIMessageHandler;

ChromeTimeTicksBrowserTest::ChromeTimeTicksBrowserTest() = default;

ChromeTimeTicksBrowserTest::~ChromeTimeTicksBrowserTest() = default;

ChromeTimeTicksBrowserTest::ChromeTimeTicksWebUIMessageHandler::
    ChromeTimeTicksWebUIMessageHandler() = default;

ChromeTimeTicksBrowserTest::ChromeTimeTicksWebUIMessageHandler::
    ~ChromeTimeTicksWebUIMessageHandler() = default;

void ChromeTimeTicksBrowserTest::ChromeTimeTicksWebUIMessageHandler::
    RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "checkTimeticks",
      base::BindRepeating(
          &ChromeTimeTicksWebUIMessageHandler::HandleCheckTimeTicks,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChromeTimeTicksBrowserTest::ChromeTimeTicksWebUIMessageHandler::
    HandleCheckTimeTicks(base::Value::ConstListView args) {
  int64_t timeTicks_in_us;
  EXPECT_TRUE(base::StringToInt64(args[0].GetString(), &timeTicks_in_us));
  // Renderer's chrome.timeTicks.nowInMicroseconds() should be close to
  // browser's base::TimeTicks::Now().
  EXPECT_LE(base::TimeTicks::Now().since_origin() -
                base::Microseconds(timeTicks_in_us),
            base::Milliseconds(100));
}

WebUIMessageHandler* ChromeTimeTicksBrowserTest::GetMockMessageHandler() {
  return &message_handler_;
}
