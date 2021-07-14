// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

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
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout);

Status CallWindowCommand(Command command,
                         const base::DictionaryValue& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  WebView* web_view = NULL;
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
                         const base::DictionaryValue& params = {},
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
  base::DictionaryValue params;
  params.SetDictionary("params", std::make_unique<base::DictionaryValue>());
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("command not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoParams) {
  base::DictionaryValue params;
  params.SetString("cmd", "CSS.enable");
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("params not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerMouse) {
  Session session("1");
  std::vector<std::unique_ptr<base::DictionaryValue>> action_list;
  std::unique_ptr<base::DictionaryValue> action_sequence(
      new base::DictionaryValue());
  std::unique_ptr<base::ListValue> actions(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> parameters(
      new base::DictionaryValue());
  parameters->SetString("pointerType", "mouse");
  action->SetString("type", "pointerMove");
  action->SetInteger("x", 30);
  action->SetInteger("y", 60);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerDown");
  action->SetInteger("button", 0);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerUp");
  action->SetInteger("button", 0);
  actions->Append(std::move(action));

  // pointer properties
  action_sequence->SetString("type", "pointer");
  action_sequence->SetString("id", "pointer1");
  action_sequence->SetDictionary("parameters", std::move(parameters));
  action_sequence->SetList("actions", std::move(actions));
  const base::DictionaryValue* input_action_sequence = action_sequence.get();
  Status status =
      ProcessInputActionSequence(&session, input_action_sequence, &action_list);
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
  action1->GetInteger("x", &x);
  action1->GetInteger("y", &y);
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
  std::unique_ptr<base::DictionaryValue> action_sequence(
      new base::DictionaryValue());
  std::unique_ptr<base::ListValue> actions(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> parameters(
      new base::DictionaryValue());
  parameters->SetString("pointerType", "touch");
  action->SetString("type", "pointerMove");
  action->SetInteger("x", 30);
  action->SetInteger("y", 60);
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerDown");
  actions->Append(std::move(action));
  action = std::make_unique<base::DictionaryValue>();
  action->SetString("type", "pointerUp");
  actions->Append(std::move(action));

  // pointer properties
  action_sequence->SetString("type", "pointer");
  action_sequence->SetString("id", "pointer1");
  action_sequence->SetDictionary("parameters", std::move(parameters));
  action_sequence->SetList("actions", std::move(actions));
  const base::DictionaryValue* input_action_sequence = action_sequence.get();
  Status status =
      ProcessInputActionSequence(&session, input_action_sequence, &action_list);
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
  action1->GetInteger("x", &x);
  action1->GetInteger("y", &y);
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
  explicit AddCookieWebView(std::string documentUrl)
      : StubWebView("1"), documentUrl_(documentUrl) {}
  ~AddCookieWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function.find("document.URL") != std::string::npos) {
      *result = std::make_unique<base::Value>(documentUrl_);
    }
    return Status(kOk);
  }

 private:
  std::string documentUrl_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteAddCookie_Valid) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "Strict");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_NameMissing) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "invalid");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'name'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_MissingValue) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("sameSite", "Strict");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_DomainInvalid) {
  AddCookieWebView webview = AddCookieWebView("file://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "Strict");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidCookieDomain, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteEmpty) {
  AddCookieWebView webview = AddCookieWebView("https://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  cookie_params->SetString("sameSite", "");
  params.SetDictionary("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteNotSet) {
  AddCookieWebView webview = AddCookieWebView("ftp://chromium.org");
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> cookie_params =
      std::make_unique<base::DictionaryValue>();
  cookie_params->SetString("name", "testcookie");
  cookie_params->SetString("value", "cookievalue");
  params.SetDictionary("cookie", std::move(cookie_params));
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

  Status PrintToPDF(const base::DictionaryValue& params,
                    std::string* pdf) override {
    params_ = params.Clone();
    return Status(kOk);
  }

  const base::Value& getParams() const { return params_; }

 private:
  base::Value params_;
};

base::DictionaryValue getDefaultPrintParams() {
  base::DictionaryValue printParams;
  printParams.SetBoolean("landscape", false);
  printParams.SetDoubleKey("scale", 1.0);
  printParams.SetDoubleKey("marginBottom", ConvertCentimeterToInch(1.0));
  printParams.SetDoubleKey("marginLeft", ConvertCentimeterToInch(1.0));
  printParams.SetDoubleKey("marginRight", ConvertCentimeterToInch(1.0));
  printParams.SetDoubleKey("marginTop", ConvertCentimeterToInch(1.0));
  printParams.SetDoubleKey("paperHeight", ConvertCentimeterToInch(27.94));
  printParams.SetDoubleKey("paperWidth", ConvertCentimeterToInch(21.59));
  printParams.SetString("pageRanges", "");
  printParams.SetBoolean("preferCSSPageSize", false);
  printParams.SetBoolean("printBackground", false);
  printParams.SetString("transferMode", "ReturnAsBase64");
  return printParams;
}
}  // namespace

TEST(WindowCommandsTest, ExecutePrintDefaultParams) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyOrientation) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  params.SetString("orientation", "portrait");
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetString("orientation", "landscape");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetBoolean("landscape", true);
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetString("orientation", "Invalid");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.SetBoolean("orientation", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyScale) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  params.SetDoubleKey("scale", 1.0);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetDoubleKey("scale", 2.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("scale", 2.0);
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetDoubleKey("scale", 0.05);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.SetDoubleKey("scale", 2.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.SetString("scale", "1.3");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyBackground) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  params.SetBoolean("background", false);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetBoolean("background", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetBoolean("printBackground", true);
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetString("background", "true");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.SetInteger("background", 2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyShrinkToFit) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  params.SetBoolean("shrinkToFit", true);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetBoolean("shrinkToFit", false);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetBoolean("preferCSSPageSize", true);
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  params.SetString("shrinkToFit", "False");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.SetInteger("shrinkToFit", 2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPageRanges) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  std::unique_ptr<base::ListValue> lv(new base::ListValue());
  params.SetList("pageRanges", std::move(lv));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  lv = std::make_unique<base::ListValue>();
  lv->AppendInteger(2);
  lv->AppendInteger(1);
  lv->AppendInteger(3);
  lv->AppendString("4-4");
  lv->AppendString("4-");
  lv->AppendString("-5");
  params.SetList("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetString("pageRanges", "2,1,3,4-4,4-,-5");
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  lv = std::make_unique<base::ListValue>();
  lv->AppendInteger(-1);
  params.SetList("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  lv = std::make_unique<base::ListValue>();
  lv->Append(3.0);
  params.SetList("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  lv = std::make_unique<base::ListValue>();
  lv->AppendBoolean(true);
  params.SetList("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  // ExecutePrint delegates invalid string checks to CDP
  lv = std::make_unique<base::ListValue>();
  lv->AppendString("-");
  lv->AppendString("");
  lv->AppendString("  ");
  lv->AppendString(" 1-3 ");
  lv->AppendString("Invalid");
  params.SetList("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetString("pageRanges", "-,,  , 1-3 ,Invalid");
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPage) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  std::unique_ptr<base::DictionaryValue> dv(new base::DictionaryValue());
  params.SetDictionary("page", std::move(dv));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("width", 21.59);
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("paperWidth", ConvertCentimeterToInch(21.59));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("width", 33);
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("paperWidth", ConvertCentimeterToInch(33));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetString("width", "10");
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("width", -3.0);
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("height", 20);
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("paperHeight", ConvertCentimeterToInch(20));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("height", 27.94);
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("paperHeight", ConvertCentimeterToInch(27.94));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetString("height", "10");
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("height", -3.0);
  params.SetDictionary("page", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyMargin) {
  StorePrintParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;

  std::unique_ptr<base::DictionaryValue> dv(new base::DictionaryValue());
  params.SetDictionary("margin", std::move(dv));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue printParams = getDefaultPrintParams();
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("top", 1.0);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginTop", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("top", 10.2);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginTop", ConvertCentimeterToInch(10.2));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetString("top", "10.2");
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("top", -0.1);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("bottom", 1.0);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginBottom", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("bottom", 5.3);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginBottom", ConvertCentimeterToInch(5.3));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetString("bottom", "10.2");
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("bottom", -0.1);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("left", 1.0);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginLeft", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("left", 9.1);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginLeft", ConvertCentimeterToInch(9.1));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetString("left", "10.2");
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("left", -0.1);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("right", 1.0);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginRight", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("right", 8.1);
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  printParams = getDefaultPrintParams();
  printParams.SetDoubleKey("marginRight", ConvertCentimeterToInch(8.1));
  ASSERT_EQ(static_cast<const base::Value&>(printParams), webview.getParams());

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetString("right", "10.2");
  params.SetDictionary("margin", std::move(dv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  dv = std::make_unique<base::DictionaryValue>();
  dv->SetDoubleKey("right", -0.1);
  params.SetDictionary("margin", std::move(dv));
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
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) override {
    if (cmd == "Page.getLayoutMetrics") {
      std::unique_ptr<base::DictionaryValue> res =
          std::make_unique<base::DictionaryValue>();
      std::unique_ptr<base::DictionaryValue> d =
          std::make_unique<base::DictionaryValue>();
      d->SetDoubleKey("width", wd);
      d->SetDoubleKey("height", hd);
      res->SetDictionary("contentSize", std::move(d));
      *value = std::move(res);
    } else if (cmd == "Emulation.setDeviceMetricsOverride") {
      base::DictionaryValue expect;
      expect.SetInteger("width", wi);
      expect.SetInteger("height", hi);
      if (meom_->HasOverrideMetrics()) {
        expect.SetInteger("deviceScaleFactor", device_scale_factor);
        expect.SetBoolean("mobile", mobile);
      } else {
        expect.SetInteger("deviceScaleFactor", 1);
        expect.SetBoolean("mobile", false);
      }
      if (expect != params)
        return Status(kInvalidArgument);
    }

    return Status(kOk);
  }

  Status CaptureScreenshot(std::string* screenshot,
                           const base::DictionaryValue& params) override {
    params_ = params.Clone();
    return Status(kOk);
  }

  const base::Value& getParams() const { return params_; }

  MobileEmulationOverrideManager* GetMobileEmulationOverrideManager()
      const override {
    return meom_.get();
  }

 private:
  base::Value params_;
  std::unique_ptr<MobileEmulationOverrideManager> meom_;
};

base::DictionaryValue getExpectedCaptureParams() {
  base::DictionaryValue clip;
  return clip;
}
}  // namespace

TEST(WindowCommandsTest, ExecuteScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteScreenshot, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictionaryValue screenshotParams = base::DictionaryValue();
  ASSERT_EQ(static_cast<const base::Value&>(screenshotParams),
            webview.getParams());
}

TEST(WindowCommandsTest, ExecuteFullPageScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(static_cast<const base::Value&>(getExpectedCaptureParams()),
            webview.getParams());
}

TEST(WindowCommandsTest, ExecuteMobileFullPageScreenCapture) {
  StubDevToolsClient sdtc;
  DeviceMetrics dm(0, 0, device_scale_factor, false, mobile);
  StoreScreenshotParamsWebView webview(&sdtc, &dm);
  ASSERT_EQ(webview.GetMobileEmulationOverrideManager()->HasOverrideMetrics(),
            true);
  base::DictionaryValue params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(static_cast<const base::Value&>(getExpectedCaptureParams()),
            webview.getParams());
}
