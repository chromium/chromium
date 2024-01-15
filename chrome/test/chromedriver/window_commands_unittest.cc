// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override {}

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
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout);

Status CallWindowCommand(Command command,
                         const base::Value::Dict& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  WebView* web_view = nullptr;
  Status status = chrome->GetWebViewById("1", &web_view);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> local_value;
  Timeout timeout;
  return command(&session, web_view, params, value ? value : &local_value,
                 &timeout);
}

Status CallWindowCommand(Command command,
                         StubWebView* web_view,
                         const base::Value::Dict& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  std::unique_ptr<base::Value> local_value;
  Timeout timeout;
  return command(&session, web_view, params, value ? value : &local_value,
                 &timeout);
}

}  // namespace

TEST(WindowCommandsTest, ExecuteFreeze) {
  Status status = CallWindowCommand(ExecuteFreeze);
  ASSERT_EQ(kOk, status.code());
}

TEST(WindowCommandsTest, ExecuteResume) {
  Status status = CallWindowCommand(ExecuteResume);
  ASSERT_EQ(kOk, status.code());
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoCmd) {
  base::Value::Dict params;
  params.Set("params", base::Value::Dict());
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("command not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoParams) {
  base::Value::Dict params;
  params.Set("cmd", "CSS.enable");
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("params not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerMouse) {
  Session session("1");
  std::vector<base::Value::Dict> action_list;
  base::Value::Dict action_sequence;
  base::Value::List actions;
  base::Value::Dict parameters;
  parameters.Set("pointerType", "mouse");
  action_sequence.Set("parameters", std::move(parameters));
  {
    base::Value::Dict action;
    action.Set("type", "pointerMove");
    action.Set("x", 30);
    action.Set("y", 60);
    actions.Append(std::move(action));
  }
  {
    base::Value::Dict action;
    action.Set("type", "pointerDown");
    action.Set("button", 0);
    actions.Append(std::move(action));
  }
  {
    base::Value::Dict action;
    action.Set("type", "pointerUp");
    action.Set("button", 0);
    actions.Append(std::move(action));
  }

  // pointer properties
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  action_sequence.Set("actions", std::move(actions));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  ASSERT_EQ(3U, action_list.size());
  const base::Value::Dict& action1 = action_list[0];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action1.FindString("type")));
  ASSERT_EQ("mouse", base::OptionalFromPtr(action1.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action1.FindString("id")));
  ASSERT_EQ("pointerMove",
            base::OptionalFromPtr(action1.FindString("subtype")));
  ASSERT_EQ(30, action1.FindInt("x"));
  ASSERT_EQ(60, action1.FindInt("y"));

  const base::Value::Dict& action2 = action_list[1];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action2.FindString("type")));
  ASSERT_EQ("mouse", base::OptionalFromPtr(action2.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action2.FindString("id")));
  ASSERT_EQ("pointerDown",
            base::OptionalFromPtr(action2.FindString("subtype")));
  ASSERT_EQ("left", base::OptionalFromPtr(action2.FindString("button")));

  const base::Value::Dict& action3 = action_list[2];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action3.FindString("type")));
  ASSERT_EQ("mouse", base::OptionalFromPtr(action3.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action3.FindString("id")));
  ASSERT_EQ("pointerUp", base::OptionalFromPtr(action3.FindString("subtype")));
  ASSERT_EQ("left", base::OptionalFromPtr(action3.FindString("button")));
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerTouch) {
  Session session("1");
  std::vector<base::Value::Dict> action_list;
  base::Value::Dict action_sequence;
  base::Value::List actions;
  base::Value::Dict parameters;
  parameters.Set("pointerType", "touch");
  action_sequence.Set("parameters", std::move(parameters));
  {
    base::Value::Dict action;
    action.Set("type", "pointerMove");
    action.Set("x", 30);
    action.Set("y", 60);
    actions.Append(std::move(action));
  }
  {
    base::Value::Dict action;
    action.Set("type", "pointerDown");
    actions.Append(std::move(action));
  }
  {
    base::Value::Dict action;
    action.Set("type", "pointerUp");
    actions.Append(std::move(action));
  }

  // pointer properties
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  action_sequence.Set("actions", std::move(actions));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  ASSERT_EQ(3U, action_list.size());
  const base::Value::Dict& action1 = action_list[0];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action1.FindString("type")));
  ASSERT_EQ("touch", base::OptionalFromPtr(action1.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action1.FindString("id")));
  ASSERT_EQ("pointerMove",
            base::OptionalFromPtr(action1.FindString("subtype")));
  ASSERT_EQ(30, action1.FindInt("x"));
  ASSERT_EQ(60, action1.FindInt("y"));

  const base::Value::Dict& action2 = action_list[1];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action2.FindString("type")));
  ASSERT_EQ("touch", base::OptionalFromPtr(action2.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action2.FindString("id")));
  ASSERT_EQ("pointerDown",
            base::OptionalFromPtr(action2.FindString("subtype")));

  const base::Value::Dict& action3 = action_list[2];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action3.FindString("type")));
  ASSERT_EQ("touch", base::OptionalFromPtr(action3.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action3.FindString("id")));
  ASSERT_EQ("pointerUp", base::OptionalFromPtr(action3.FindString("subtype")));
}

TEST(WindowCommandsTest, ExecuteSetRPHRegistrationMode_NoParams) {
  base::Value::Dict params;
  Status status = CallWindowCommand(ExecuteSetRPHRegistrationMode, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("missing parameter 'mode'"),
            std::string::npos);
}

TEST(WindowCommandsTest, ExecuteSetRPHRegistrationMode) {
  base::Value::Dict params;
  params.Set("mode", "autoaccept");
  Status status = CallWindowCommand(ExecuteSetRPHRegistrationMode, params);
  ASSERT_EQ(kOk, status.code());
}

namespace {

class AddCookieWebView : public StubWebView {
 public:
  explicit AddCookieWebView(std::string document_url)
      : StubWebView("1"), document_url_(document_url) {}
  ~AddCookieWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::Value::List& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function.find("document.URL") != std::string::npos) {
      *result = std::make_unique<base::Value>(document_url_);
    }
    return Status(kOk);
  }

 private:
  std::string document_url_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteAddCookie_Valid) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::Value::Dict params;
  base::Value::Dict cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "Strict");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_NameMissing) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::Value::Dict params;
  base::Value::Dict cookie_params;
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "invalid");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'name'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_MissingValue) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::Value::Dict params;
  base::Value::Dict cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("sameSite", "Strict");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_DomainInvalid) {
  AddCookieWebView webview = AddCookieWebView("file://chromium.org");
  base::Value::Dict params;
  base::Value::Dict cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "Strict");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidCookieDomain, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteEmpty) {
  AddCookieWebView webview = AddCookieWebView("https://chromium.org");
  base::Value::Dict params;
  base::Value::Dict cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteNotSet) {
  AddCookieWebView webview = AddCookieWebView("ftp://chromium.org");
  base::Value::Dict params;
  base::Value::Dict cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

namespace {

class GetCookiesWebView : public StubWebView {
 public:
  explicit GetCookiesWebView(std::string document_url)
      : StubWebView("1"), document_url_(document_url) {}
  ~GetCookiesWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::Value::List& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function.find("document.URL") != std::string::npos) {
      *result = std::make_unique<base::Value>(document_url_);
    }
    return Status(kOk);
  }

  Status GetCookies(base::Value* cookies,
                    const std::string& current_page_url) override {
    base::Value::List new_cookies;
    base::Value::Dict cookie_0;
    cookie_0.Set("name", "a");
    cookie_0.Set("value", "0");
    cookie_0.Set("domain", "example.com");
    cookie_0.Set("path", "/");
    cookie_0.Set("session", true);
    new_cookies.Append(cookie_0.Clone());
    base::Value::Dict cookie_1;
    cookie_1.Set("name", "b");
    cookie_1.Set("value", "1");
    cookie_1.Set("domain", "example.org");
    cookie_1.Set("path", "/test");
    cookie_1.Set("sameSite", "None");
    cookie_1.Set("expires", 10);
    cookie_1.Set("httpOnly", true);
    cookie_1.Set("session", false);
    cookie_1.Set("secure", true);
    new_cookies.Append(cookie_1.Clone());
    *cookies = base::Value(new_cookies.Clone());
    return Status(kOk);
  }

 private:
  std::string document_url_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteGetCookies) {
  GetCookiesWebView webview = GetCookiesWebView("https://chromium.org");
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteGetCookies, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::List expected_cookies;
  base::Value::Dict cookie_0;
  cookie_0.Set("name", "a");
  cookie_0.Set("value", "0");
  cookie_0.Set("domain", "example.com");
  cookie_0.Set("path", "/");
  cookie_0.Set("sameSite", "Lax");
  cookie_0.Set("httpOnly", false);
  cookie_0.Set("secure", false);
  expected_cookies.Append(cookie_0.Clone());
  base::Value::Dict cookie_1;
  cookie_1.Set("name", "b");
  cookie_1.Set("value", "1");
  cookie_1.Set("domain", "example.org");
  cookie_1.Set("path", "/test");
  cookie_1.Set("sameSite", "None");
  cookie_1.Set("expiry", 10);
  cookie_1.Set("httpOnly", true);
  cookie_1.Set("secure", true);
  expected_cookies.Append(cookie_1.Clone());
  EXPECT_EQ(result_value->GetList(), expected_cookies);
}

TEST(WindowCommandsTest, ExecuteGetNamedCookie) {
  GetCookiesWebView webview = GetCookiesWebView("https://chromium.org");
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  // Get without cookie name.
  Status status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  // Get with undefined cookie.
  params.Set("name", "missing");
  status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kNoSuchCookie, status.code()) << status.message();

  // Get cookie a.
  params.Set("name", "a");
  status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict expected_cookie_0;
  expected_cookie_0.Set("name", "a");
  expected_cookie_0.Set("value", "0");
  expected_cookie_0.Set("domain", "example.com");
  expected_cookie_0.Set("path", "/");
  expected_cookie_0.Set("sameSite", "Lax");
  expected_cookie_0.Set("httpOnly", false);
  expected_cookie_0.Set("secure", false);
  EXPECT_EQ(result_value->GetDict(), expected_cookie_0);

  // Get cookie b.
  params.Set("name", "b");
  status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict expected_cookie_1;
  expected_cookie_1.Set("name", "b");
  expected_cookie_1.Set("value", "1");
  expected_cookie_1.Set("domain", "example.org");
  expected_cookie_1.Set("path", "/test");
  expected_cookie_1.Set("sameSite", "None");
  expected_cookie_1.Set("expiry", 10);
  expected_cookie_1.Set("httpOnly", true);
  expected_cookie_1.Set("secure", true);
  EXPECT_EQ(result_value->GetDict(), expected_cookie_1);
}

namespace {

class StorePrintParamsWebView : public StubWebView {
 public:
  StorePrintParamsWebView() : StubWebView("1") {}
  ~StorePrintParamsWebView() override = default;

  Status PrintToPDF(const base::Value::Dict& params,
                    std::string* pdf) override {
    params_ = base::Value(params.Clone());
    return Status(kOk);
  }

  const base::Value& GetParams() const { return params_; }

 private:
  base::Value params_;
};

base::Value::Dict GetDefaultPrintParams() {
  base::Value::Dict dict;
  dict.Set("landscape", false);
  dict.Set("scale", 1.0);
  dict.Set("marginBottom", ConvertCentimeterToInch(1.0));
  dict.Set("marginLeft", ConvertCentimeterToInch(1.0));
  dict.Set("marginRight", ConvertCentimeterToInch(1.0));
  dict.Set("marginTop", ConvertCentimeterToInch(1.0));
  dict.Set("paperHeight", ConvertCentimeterToInch(27.94));
  dict.Set("paperWidth", ConvertCentimeterToInch(21.59));
  dict.Set("pageRanges", "");
  dict.Set("preferCSSPageSize", false);
  dict.Set("printBackground", false);
  dict.Set("transferMode", "ReturnAsBase64");
  return dict;
}
}  // namespace

TEST(WindowCommandsTest, ExecutePrintDefaultParams) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyOrientation) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("orientation", "portrait");
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("orientation", "landscape");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("landscape", true);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("orientation", "Invalid");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("orientation", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyScale) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("scale", 1.0);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("scale", 2.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("scale", 2.0);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("scale", 0.05);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("scale", 2.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("scale", "1.3");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyBackground) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("background", false);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("background", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("printBackground", true);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("background", "true");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("background", 2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyShrinkToFit) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("shrinkToFit", true);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("shrinkToFit", false);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("preferCSSPageSize", true);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("shrinkToFit", "False");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("shrinkToFit", 2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPageRanges) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  base::Value::List lv;
  params.Set("pageRanges", std::move(lv));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  lv = base::Value::List();
  lv.Append(2);
  lv.Append(1);
  lv.Append(3);
  lv.Append("4-4");
  lv.Append("4-");
  lv.Append("-5");
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("pageRanges", "2,1,3,4-4,4-,-5");
  ASSERT_EQ(print_params, webview.GetParams());

  lv = base::Value::List();
  lv.Append(-1);
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  lv = base::Value::List();
  lv.Append(3.0);
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  lv = base::Value::List();
  lv.Append(true);
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  // ExecutePrint delegates invalid string checks to CDP
  lv = base::Value::List();
  lv.Append("-");
  lv.Append("");
  lv.Append("  ");
  lv.Append(" 1-3 ");
  lv.Append("Invalid");
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("pageRanges", "-,,  , 1-3 ,Invalid");
  ASSERT_EQ(print_params, webview.GetParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPage) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("page", base::Value::Dict());
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("width", 21.59);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperWidth", ConvertCentimeterToInch(21.59));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("width", 33);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperWidth", ConvertCentimeterToInch(33));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("width", "10");
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("width", -3.0);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("height", 20);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperHeight", ConvertCentimeterToInch(20));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("height", 27.94);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperHeight", ConvertCentimeterToInch(27.94));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("height", "10");
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("height", -3.0);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyMargin) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("margin", base::Value::Dict());
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("top", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginTop", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("top", 10.2);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginTop", ConvertCentimeterToInch(10.2));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("top", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("top", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("bottom", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginBottom", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("bottom", 5.3);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginBottom", ConvertCentimeterToInch(5.3));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("bottom", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("bottom", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("left", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginLeft", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("left", 9.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginLeft", ConvertCentimeterToInch(9.1));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("left", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("left", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("right", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginRight", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("right", 8.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginRight", ConvertCentimeterToInch(8.1));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::Value::Dict dv;
    dv.Set("right", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::Value::Dict dv;
    dv.Set("right", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

namespace {
constexpr double wd = 345.6;
constexpr double hd = 5432.1;
constexpr int wi = 346;
constexpr int hi = 5433;
constexpr bool mobile = false;
constexpr double device_scale_factor = 0.3;

class StoreScreenshotParamsWebView : public StubWebView {
 public:
  explicit StoreScreenshotParamsWebView(
      DevToolsClient* dtc = nullptr,
      std::optional<MobileDevice> md = std::nullopt)
      : StubWebView("1"),
        meom_(new MobileEmulationOverrideManager(dtc, md, 0)) {}
  ~StoreScreenshotParamsWebView() override = default;

  Status SendCommandAndGetResult(const std::string& cmd,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) override {
    if (cmd == "Page.getLayoutMetrics") {
      base::Value::Dict res;
      base::Value::Dict d;
      d.Set("width", wd);
      d.Set("height", hd);
      res.Set("contentSize", std::move(d));
      *value = std::make_unique<base::Value>(std::move(res));
    } else if (cmd == "Emulation.setDeviceMetricsOverride") {
      base::Value::Dict expect;
      expect.Set("width", wi);
      expect.Set("height", hi);
      if (meom_->HasOverrideMetrics()) {
        expect.Set("deviceScaleFactor", device_scale_factor);
        expect.Set("mobile", mobile);
      } else {
        expect.Set("deviceScaleFactor", 1);
        expect.Set("mobile", false);
      }
      if (expect != params)
        return Status(kInvalidArgument);
    }

    return Status(kOk);
  }

  Status CaptureScreenshot(std::string* screenshot,
                           const base::Value::Dict& params) override {
    params_ = base::Value(params.Clone());
    return Status(kOk);
  }

  const base::Value& GetParams() const { return params_; }

  MobileEmulationOverrideManager* GetMobileEmulationOverrideManager()
      const override {
    return meom_.get();
  }

 private:
  base::Value params_;
  std::unique_ptr<MobileEmulationOverrideManager> meom_;
};

base::Value::Dict GetExpectedCaptureParams() {
  base::Value::Dict clip;
  return clip;
}
}  // namespace

TEST(WindowCommandsTest, ExecuteScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteScreenshot, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::Value::Dict screenshot_params;
  ASSERT_EQ(screenshot_params, webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteFullPageScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(GetExpectedCaptureParams(), webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteMobileFullPageScreenCapture) {
  StubDevToolsClient sdtc;
  MobileDevice mobile_device;
  mobile_device.device_metrics =
      DeviceMetrics(0, 0, device_scale_factor, false, mobile);
  StoreScreenshotParamsWebView webview(&sdtc, std::move(mobile_device));
  ASSERT_EQ(webview.GetMobileEmulationOverrideManager()->HasOverrideMetrics(),
            true);
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(GetExpectedCaptureParams(), webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteScript_NoScript) {
  base::Value::Dict params;
  params.Set("args", base::Value::List());
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteScript_ScriptNotAString) {
  base::Value::Dict params;
  params.Set("script", base::Value::Dict());
  params.Set("args", base::Value::List());
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteScript_NoArgs) {
  base::Value::Dict params;
  params.Set("script", "irrelevant");
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteScript_ArgsNotAList) {
  base::Value::Dict params;
  params.Set("script", "irrelevant");
  params.Set("args", "not-a-list");
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_NoScript) {
  base::Value::Dict params;
  params.Set("args", base::Value::List());
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_ScriptNotAString) {
  base::Value::Dict params;
  params.Set("script", base::Value::Dict());
  params.Set("args", base::Value::List());
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_NoArgs) {
  base::Value::Dict params;
  params.Set("script", "irrelevant");
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_ArgsNotAList) {
  base::Value::Dict params;
  params.Set("script", "irrelevant");
  params.Set("args", "not-a-list");
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, SendKeysToActiveElement_NoValue) {
  base::Value::Dict params;
  Status status = CallWindowCommand(ExecuteSendKeysToActiveElement, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, SendKeysToActiveElement_ValueNotAList) {
  base::Value::Dict params;
  params.Set("value", base::Value::Dict());
  Status status = CallWindowCommand(ExecuteSendKeysToActiveElement, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_NoActions) {
  base::Value::Dict params;
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' must be a list"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_ActionsNotAList) {
  base::Value::Dict params;
  params.Set("actions", 7);
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' must be a list"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_NoActionsInSequence) {
  base::Value::Dict sequence;
  sequence.Set("id", "irrelevant");
  sequence.Set("type", "none");
  base::Value::List actions;
  actions.Append(sequence.Clone());
  base::Value::Dict params;
  params.Set("actions", actions.Clone());
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' in the sequence must be a list"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_ActionsInSequenceNotAList) {
  base::Value::Dict sequence;
  sequence.Set("id", "irrelevant");
  sequence.Set("type", "none");
  sequence.Set("actions", base::Value::Dict());
  base::Value::List actions;
  actions.Append(sequence.Clone());
  base::Value::Dict params;
  params.Set("actions", actions.Clone());
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' in the sequence must be a list"),
            std::string::npos)
      << status.message();
}
