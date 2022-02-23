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
  base::Value::ConstListView list_view = list_value->GetListDeprecated();
  ASSERT_TRUE(0u < list_view.size() && list_view[0].is_string());
  std::string call_js = list_view[0].GetString();
  web_ui()->CallJavascriptFunctionUnsafe(call_js);
}

void WebUIBrowserAsyncGenTest::AsyncWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "callJS", base::BindRepeating(&AsyncWebUIMessageHandler::HandleCallJS,
                                    base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "tearDown", base::BindRepeating(&AsyncWebUIMessageHandler::HandleTearDown,
                                      base::Unretained(this)));
}
