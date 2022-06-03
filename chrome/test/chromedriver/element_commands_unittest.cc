// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webdriver/atoms.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override = default;

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  // Using a StubWebView does not allow testing the functionality end-to-end,
  // more details in crbug.com/850703
  StubWebView web_view_;
};

typedef Status (*Command)(Session* session,
                          WebView* web_view,
                          const std::string& element_id,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value);

Status CallElementCommand(Command command,
                          StubWebView* web_view,
                          const std::string& element_id,
                          const base::DictionaryValue& params = {},
                          std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  std::unique_ptr<base::Value> local_value;
  return command(&session, web_view, element_id, params,
                 value ? value : &local_value);
}

}  // namespace

namespace {

class MockResponseWebView : public StubWebView {
 public:
  MockResponseWebView() : StubWebView("1") {}
  ~MockResponseWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function ==
        webdriver::atoms::asString(webdriver::atoms::GET_LOCATION)) {
      *result = std::make_unique<base::DictionaryValue>();
      (*result)->SetIntPath("value.status", 0);
      (*result)->SetDoublePath("x", 0.0);
      (*result)->SetDoublePath("y", 128.8);
    } else if (function ==
               webdriver::atoms::asString(webdriver::atoms::GET_SIZE)) {
      // Do not set result; this should be an error state
      return Status(kStaleElementReference);
    } else {
      *result = std::make_unique<base::DictionaryValue>();
      (*result)->SetIntPath("value.status", 0);
    }
    return Status(kOk);
  }
};

}  // namespace

TEST(ElementCommandsTest, ExecuteGetElementRect_SizeError) {
  MockResponseWebView webview = MockResponseWebView();
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteGetElementRect, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     params, &result_value);
  ASSERT_EQ(kStaleElementReference, status.code()) << status.message();
}
