// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/chrome_send_browsertest.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::WebUIMessageHandler;

ChromeSendWebUITest::ChromeSendWebUITest() {}

ChromeSendWebUITest::~ChromeSendWebUITest() {}

ChromeSendWebUITest::ChromeSendWebUIMessageHandler::
    ChromeSendWebUIMessageHandler() {}

ChromeSendWebUITest::ChromeSendWebUIMessageHandler::
    ~ChromeSendWebUIMessageHandler() {}

void ChromeSendWebUITest::ChromeSendWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "checkSend",
      base::BindRepeating(&ChromeSendWebUIMessageHandler::HandleCheckSend,
                          base::Unretained(this)));
}

WebUIMessageHandler* ChromeSendWebUITest::GetMockMessageHandler() {
  return &message_handler_;
}

ChromeSendPassthroughWebUITest::ChromeSendPassthroughWebUITest() {}

ChromeSendPassthroughWebUITest::~ChromeSendPassthroughWebUITest() {}

void ChromeSendPassthroughWebUITest::SetUpOnMainThread() {
  ChromeSendWebUITest::SetUpOnMainThread();
  EXPECT_CALL(message_handler_, HandleCheckSend(::testing::_));
}
