// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
  params.Set("params", base::Value(base::Value::Type::DICTIONARY));
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
  std::vector<std::unique_ptr<base::DictionaryValue>> action_list;
  base::Value::Dict action_sequence;
  base::Value::List actions;
  base::Value action(base::Value::Type::DICTIONARY);
  base::Value* parameters = action_sequence.Set(
      "parameters", base::Value(base::Value::Type::DICTIONARY));
  parameters->GetDict().Set("pointerType", "mouse");
  base::Value::Dict& action_dict = action.GetDict();
  action_dict.Set("type", "pointerMove");
  action_dict.Set("x", 30);
  action_dict.Set("y", 60);
  actions.Append(std::move(action));
  action = base::Value(base::Value::Type::DICTIONARY);
  action_dict.Set("type", "pointerDown");
  action_dict.Set("button", 0);
  actions.Append(std::move(action));
  action = base::Value(base::Value::Type::DICTIONARY);
  action_dict.Set("type", "pointerUp");
  action_dict.Set("button", 0);
  actions.Append(std::move(action));

  // pointer properties
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  action_sequence.Set("actions", std::move(actions));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  std::string pointer_type;
  std::string source_type;
  std::string id;
  std::string action_type;
  int x, y;
  std::string button;

  ASSERT_EQ(3U, action_list.size());
  const base::DictionaryValue* action1 = action_list[0].get();
  action1->GetString("type", &source_type);
  action1->GetString("pointerType", &pointer_type);
  action1->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);
  action1->GetString("subtype", &action_type);
  x = action1->GetDict().FindInt("x").value_or(-1);
  y = action1->GetDict().FindInt("y").value_or(-1);
  ASSERT_EQ("pointerMove", action_type);
  ASSERT_EQ(30, x);
  ASSERT_EQ(60, y);

  const base::DictionaryValue* action2 = action_list[1].get();
  action2->GetString("type", &source_type);
  action2->GetString("pointerType", &pointer_type);
  action2->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);
  action2->GetString("subtype", &action_type);
  action2->GetString("button", &button);
  ASSERT_EQ("pointerDown", action_type);
  ASSERT_EQ("left", button);

  const base::DictionaryValue* action3 = action_list[2].get();
  action3->GetString("type", &source_type);
  action3->GetString("pointerType", &pointer_type);
  action3->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("mouse", pointer_type);
  ASSERT_EQ("pointer1", id);
  action3->GetString("subtype", &action_type);
  action3->GetString("button", &button);
  ASSERT_EQ("pointerUp", action_type);
  ASSERT_EQ("left", button);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerTouch) {
  Session session("1");
  std::vector<std::unique_ptr<base::DictionaryValue>> action_list;
  base::Value::Dict action_sequence;
  base::Value::List actions;
  base::Value action(base::Value::Type::DICTIONARY);
  base::Value::Dict& action_dict = action.GetDict();
  base::Value* parameters = action_sequence.Set(
      "parameters", base::Value(base::Value::Type::DICTIONARY));
  parameters->GetDict().Set("pointerType", "touch");
  action_dict.Set("type", "pointerMove");
  action_dict.Set("x", 30);
  action_dict.Set("y", 60);
  actions.Append(std::move(action));
  action = base::Value(base::Value::Type::DICTIONARY);
  action_dict.Set("type", "pointerDown");
  actions.Append(std::move(action));
  action = base::Value(base::Value::Type::DICTIONARY);
  action_dict.Set("type", "pointerUp");
  actions.Append(std::move(action));

  // pointer properties
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  action_sequence.Set("actions", std::move(actions));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  std::string pointer_type;
  std::string source_type;
  std::string id;
  std::string action_type;
  int x, y;

  ASSERT_EQ(3U, action_list.size());
  const base::DictionaryValue* action1 = action_list[0].get();
  action1->GetString("type", &source_type);
  action1->GetString("pointerType", &pointer_type);
  action1->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("touch", pointer_type);
  ASSERT_EQ("pointer1", id);
  action1->GetString("subtype", &action_type);
  x = action1->GetDict().FindInt("x").value_or(-1);
  y = action1->GetDict().FindInt("y").value_or(-1);
  ASSERT_EQ("pointerMove", action_type);
  ASSERT_EQ(30, x);
  ASSERT_EQ(60, y);

  const base::DictionaryValue* action2 = action_list[1].get();
  action2->GetString("type", &source_type);
  action2->GetString("pointerType", &pointer_type);
  action2->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("touch", pointer_type);
  ASSERT_EQ("pointer1", id);
  action2->GetString("subtype", &action_type);
  ASSERT_EQ("pointerDown", action_type);

  const base::DictionaryValue* action3 = action_list[2].get();
  action3->GetString("type", &source_type);
  action3->GetString("pointerType", &pointer_type);
  action3->GetString("id", &id);
  ASSERT_EQ("pointer", source_type);
  ASSERT_EQ("touch", pointer_type);
  ASSERT_EQ("pointer1", id);
  action3->GetString("subtype", &action_type);
  ASSERT_EQ("pointerUp", action_type);
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
  base::Value* cookie_params =
      params.Set("cookie", base::Value(base::Value::Type::DICTIONARY));
  cookie_params->GetDict().Set("name", "testcookie");
  cookie_params->GetDict().Set("value", "cookievalue");
  cookie_params->GetDict().Set("sameSite", "Strict");
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_NameMissing) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::Value::Dict params;
  base::Value* cookie_params =
      params.Set("cookie", base::Value(base::Value::Type::DICTIONARY));
  cookie_params->GetDict().Set("value", "cookievalue");
  cookie_params->GetDict().Set("sameSite", "invalid");
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
  base::Value* cookie_params =
      params.Set("cookie", base::Value(base::Value::Type::DICTIONARY));
  cookie_params->GetDict().Set("name", "testcookie");
  cookie_params->GetDict().Set("sameSite", "Strict");
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
  base::Value* cookie_params =
      params.Set("cookie", base::Value(base::Value::Type::DICTIONARY));
  cookie_params->GetDict().Set("name", "testcookie");
  cookie_params->GetDict().Set("value", "cookievalue");
  cookie_params->GetDict().Set("sameSite", "Strict");
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidCookieDomain, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteEmpty) {
  AddCookieWebView webview = AddCookieWebView("https://chromium.org");
  base::Value::Dict params;
  base::Value* cookie_params =
      params.Set("cookie", base::Value(base::Value::Type::DICTIONARY));
  cookie_params->GetDict().Set("name", "testcookie");
  cookie_params->GetDict().Set("value", "cookievalue");
  cookie_params->GetDict().Set("sameSite", "");
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteNotSet) {
  AddCookieWebView webview = AddCookieWebView("ftp://chromium.org");
  base::Value::Dict params;
  base::Value* cookie_params =
      params.Set("cookie", base::Value(base::Value::Type::DICTIONARY));
  cookie_params->GetDict().Set("name", "testcookie");
  cookie_params->GetDict().Set("value", "cookievalue");
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
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

base::DictionaryValue GetDefaultPrintParams() {
  base::DictionaryValue print_params;
  base::Value::Dict& dict = print_params.GetDict();
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
  return print_params;
}
}  // namespace

TEST(WindowCommandsTest, ExecutePrintDefaultParams) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyOrientation) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  params.Set("orientation", "portrait");
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  params.Set("orientation", "landscape");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("landscape", true);
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

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
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  params.Set("scale", 2.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("scale", 2.0);
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

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
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  params.Set("background", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("printBackground", true);
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

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
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  params.Set("shrinkToFit", false);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("preferCSSPageSize", true);
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

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
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

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
  print_params.SetString("pageRanges", "2,1,3,4-4,4-,-5");
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

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
  print_params.SetString("pageRanges", "-,,  , 1-3 ,Invalid");
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPage) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  base::Value* dv =
      params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("width", 21.59);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("paperWidth", ConvertCentimeterToInch(21.59));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("width", 33);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("paperWidth", ConvertCentimeterToInch(33));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("width", "10");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("width", -3.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("height", 20);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("paperHeight", ConvertCentimeterToInch(20));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("height", 27.94);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("paperHeight", ConvertCentimeterToInch(27.94));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("height", "10");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("page", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("height", -3.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyMargin) {
  StorePrintParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;

  base::Value* dv =
      params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("top", 1.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginTop", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("top", 10.2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginTop", ConvertCentimeterToInch(10.2));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("top", "10.2");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("top", -0.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("bottom", 1.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginBottom", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("bottom", 5.3);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginBottom", ConvertCentimeterToInch(5.3));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("bottom", "10.2");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("bottom", -0.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("left", 1.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginLeft", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("left", 9.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginLeft", ConvertCentimeterToInch(9.1));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("left", "10.2");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("left", -0.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("right", 1.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginRight", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("right", 8.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.GetDict().Set("marginRight", ConvertCentimeterToInch(8.1));
  ASSERT_EQ(static_cast<const base::Value&>(print_params), webview.GetParams());

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("right", "10.2");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = params.Set("margin", base::Value(base::Value::Type::DICTIONARY));
  dv->GetDict().Set("right", -0.1);
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
  explicit StoreScreenshotParamsWebView(DevToolsClient* dtc = nullptr,
                                        DeviceMetrics* dm = nullptr)
      : StubWebView("1"), meom_(new MobileEmulationOverrideManager(dtc, dm)) {}
  ~StoreScreenshotParamsWebView() override = default;

  Status SendCommandAndGetResult(const std::string& cmd,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) override {
    if (cmd == "Page.getLayoutMetrics") {
      std::unique_ptr<base::DictionaryValue> res =
          std::make_unique<base::DictionaryValue>();
      base::Value* d = res->GetDict().Set(
          "contentSize", base::Value(base::Value::Type::DICTIONARY));
      d->GetDict().Set("width", wd);
      d->GetDict().Set("height", hd);
      *value = std::move(res);
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

base::DictionaryValue GetExpectedCaptureParams() {
  base::DictionaryValue clip;
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
  base::DictionaryValue screenshot_params;
  ASSERT_EQ(static_cast<const base::Value&>(screenshot_params),
            webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteFullPageScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(static_cast<const base::Value&>(GetExpectedCaptureParams()),
            webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteMobileFullPageScreenCapture) {
  StubDevToolsClient sdtc;
  DeviceMetrics dm(0, 0, device_scale_factor, false, mobile);
  StoreScreenshotParamsWebView webview(&sdtc, &dm);
  ASSERT_EQ(webview.GetMobileEmulationOverrideManager()->HasOverrideMetrics(),
            true);
  base::Value::Dict params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(static_cast<const base::Value&>(GetExpectedCaptureParams()),
            webview.GetParams());
}
