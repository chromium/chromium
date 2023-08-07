// Copyright 2013 The Chromium Authors
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
#include "third_party/selenium-atoms/atoms.h"

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
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

Status CallElementCommand(Command command,
                          StubWebView* web_view,
                          const std::string& element_id,
                          const base::Value::Dict& params = {},
                          bool w3c_compliant = true,
                          std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  session.w3c_compliant = w3c_compliant;

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
                      const base::Value::List& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function ==
        webdriver::atoms::asString(webdriver::atoms::GET_LOCATION)) {
      base::Value::Dict dict;
      dict.SetByDottedPath("value.status", 0);
      dict.Set("x", 0.0);
      dict.Set("y", 128.8);
      *result = std::make_unique<base::Value>(std::move(dict));
    } else if (function ==
               webdriver::atoms::asString(webdriver::atoms::GET_SIZE)) {
      // Do not set result; this should be an error state
      return Status(kStaleElementReference);
    } else {
      base::Value::Dict dict;
      dict.SetByDottedPath("value.status", 0);
      *result = std::make_unique<base::Value>(std::move(dict));
    }
    return Status(kOk);
  }
};

}  // namespace

TEST(ElementCommandsTest, ExecuteGetElementRect_SizeError) {
  MockResponseWebView webview = MockResponseWebView();
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteGetElementRect, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     params, true, &result_value);
  ASSERT_EQ(kStaleElementReference, status.code()) << status.message();
}

TEST(ElementCommandsTest, ExecuteSendKeysToElement_NoValue) {
  MockResponseWebView webview = MockResponseWebView();
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteSendKeysToElement, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     params, false, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}

TEST(ElementCommandsTest, ExecuteSendKeysToElement_ValueNotAList) {
  MockResponseWebView webview = MockResponseWebView();
  base::Value::Dict params;
  params.Set("value", "not-a-list");
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteSendKeysToElement, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     params, false, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}
