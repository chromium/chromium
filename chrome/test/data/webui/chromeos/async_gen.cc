// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/chromeos/async_gen.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

WebUIBrowserAsyncGenTest::WebUIBrowserAsyncGenTest() {}

WebUIBrowserAsyncGenTest::~WebUIBrowserAsyncGenTest() {}

WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::
    AsyncWebUIMessageHandler() {}

WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::
    ~AsyncWebUIMessageHandler() {}

void WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::HandleCallJS(
    const base::Value::List& list_value) {
  ASSERT_TRUE(0u < list_value.size() && list_value[0].is_string());
  std::string call_js = list_value[0].GetString();
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
