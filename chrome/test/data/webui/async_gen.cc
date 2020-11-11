// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/async_gen.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

WebUIBrowserAsyncGenTest::WebUIBrowserAsyncGenTest() {}

WebUIBrowserAsyncGenTest::~WebUIBrowserAsyncGenTest() {}

WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::
    AsyncWebUIMessageHandler() {}

WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::
    ~AsyncWebUIMessageHandler() {}

void WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::HandleCallJS(
    const base::ListValue* list_value) {
  std::string call_js;
  ASSERT_TRUE(list_value->GetString(0, &call_js));
  web_ui()->CallJavascriptFunctionUnsafe(call_js);
}

void WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "callJS", base::BindRepeating(&AsyncWebUIMessageHandler::HandleCallJS,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "tearDown", base::BindRepeating(&AsyncWebUIMessageHandler::HandleTearDown,
                                      base::Unretained(this)));
}
