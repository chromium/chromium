// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_commands.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/element_util.h"
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
  // more details in crbug.com/40579857
  StubWebView web_view_;
};

typedef Status (*Command)(Session* session,
                          WebView* web_view,
                          const std::string& element_id,
                          const base::DictValue& params,
                          std::unique_ptr<base::Value>* value);

Status CallElementCommand(Command command,
                          StubWebView* web_view,
                          const std::string& element_id,
                          const base::DictValue& params = {},
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
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function ==
        webdriver::atoms::asString(webdriver::atoms::GET_LOCATION)) {
      base::DictValue dict;
      dict.SetByDottedPath("value.status", 0);
      dict.Set("x", 0.0);
      dict.Set("y", 128.8);
      *result = std::make_unique<base::Value>(std::move(dict));
    } else if (function ==
               webdriver::atoms::asString(webdriver::atoms::GET_SIZE)) {
      // Do not set result; this should be an error state
      return Status(kStaleElementReference);
    } else {
      base::DictValue dict;
      dict.SetByDottedPath("value.status", 0);
      *result = std::make_unique<base::Value>(std::move(dict));
    }
    return Status(kOk);
  }
};

}  // namespace

TEST(ElementCommandsTest, ExecuteGetElementRect_SizeError) {
  MockResponseWebView webview = MockResponseWebView();
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteGetElementRect, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     params, true, &result_value);
  ASSERT_EQ(kStaleElementReference, status.code()) << status.message();
}

TEST(ElementCommandsTest, ExecuteSendKeysToElement_NoValue) {
  MockResponseWebView webview = MockResponseWebView();
  base::DictValue params;
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
  base::DictValue params;
  params.Set("value", "not-a-list");
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteSendKeysToElement, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     params, false, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}

namespace {

// A mock WebView that simulates a contenteditable element which is the
// active element (document.activeElement) but where document.hasFocus()
// returns false (as happens in headless mode or unfocused windows).
// This reproduces the bug where IsElementFocused() incorrectly returns false
// because it gates on document.hasFocus(), causing the caret-moving script
// (range.selectNodeContents) to run and corrupt the existing selection.
class ContentEditableWebView : public StubWebView {
 public:
  explicit ContentEditableWebView(const std::string& element_id)
      : StubWebView("1"), element_id_(element_id) {}
  ~ContentEditableWebView() override = default;

  bool caret_moved() const { return caret_moved_; }

  Status EvaluateScript(const std::string& frame,
                        const std::string& expression,
                        const bool await_promise,
                        std::unique_ptr<base::Value>* result) override {
    if (expression == "document.hasFocus()") {
      // Simulate headless/unfocused window: hasFocus returns false.
      *result = std::make_unique<base::Value>(false);
      return Status(kOk);
    }
    *result = std::make_unique<base::Value>();
    return Status(kOk);
  }

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    // IS_DISPLAYED atom: element is visible.
    if (function ==
        webdriver::atoms::asString(webdriver::atoms::IS_DISPLAYED)) {
      *result = std::make_unique<base::Value>(true);
      return Status(kOk);
    }

    // IS_ENABLED atom: element is enabled.
    if (function == webdriver::atoms::asString(webdriver::atoms::IS_ENABLED)) {
      *result = std::make_unique<base::Value>(true);
      return Status(kOk);
    }

    // GET_ATTRIBUTE atom: return appropriate values for tagName and type.
    if (function ==
        webdriver::atoms::asString(webdriver::atoms::GET_ATTRIBUTE)) {
      // args[1] is the attribute name.
      if (args.size() >= 2 && args[1].is_string()) {
        const std::string& attr = args[1].GetString();
        if (attr == "tagName") {
          *result = std::make_unique<base::Value>("div");
          return Status(kOk);
        }
        if (attr == "type") {
          *result = std::make_unique<base::Value>();  // null for div
          return Status(kOk);
        }
      }
      *result = std::make_unique<base::Value>();
      return Status(kOk);
    }

    // document.activeElement: return the target element.
    if (function.find("document.activeElement") != std::string::npos) {
      *result = std::make_unique<base::Value>(
          CreateElement(element_id_, /*w3c_compliant=*/true));
      return Status(kOk);
    }

    // element.isContentEditable: return true.
    if (function.find("isContentEditable") != std::string::npos &&
        function.find("parentElement") == std::string::npos) {
      *result = std::make_unique<base::Value>(true);
      return Status(kOk);
    }

    // Parent traversal for top-level contenteditable: return same element.
    if (function.find("parentElement") != std::string::npos) {
      *result = std::make_unique<base::Value>(
          CreateElement(element_id_, /*w3c_compliant=*/true));
      return Status(kOk);
    }

    // Caret-moving script: range.selectNodeContents / range.collapse.
    if (function.find("selectNodeContents") != std::string::npos) {
      caret_moved_ = true;
      *result = std::make_unique<base::Value>();
      return Status(kOk);
    }

    // Default: handles kFocusScript and any other CallFunction calls.
    *result = std::make_unique<base::Value>();
    return Status(kOk);
  }

  Status DispatchKeyEvents(const std::vector<KeyEvent>& events,
                           bool async_dispatch_events) override {
    return Status(kOk);
  }

 private:
  std::string element_id_;
  bool caret_moved_ = false;
};

}  // namespace

// Tests that SendKeys to a contenteditable element that is already the
// active element does NOT move the caret, even when document.hasFocus()
// returns false (headless mode). This is the bug fixed by using
// IsActiveElement() instead of IsElementFocused().
// See: https://crbug.com/chromedriver/4920
TEST(ElementCommandsTest,
     ExecuteSendKeysToElement_ContentEditablePreservesCaret) {
  const std::string element_id = "test-element-id";
  ContentEditableWebView webview(element_id);
  base::DictValue params;
  params.Set("text", "x");
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteSendKeysToElement, &webview,
                                     element_id, params, true, &result_value);
  ASSERT_TRUE(status.IsOk()) << status.message();
  // The caret-moving script (range.selectNodeContents) should NOT have been
  // called because the element is already the active element.
  EXPECT_FALSE(webview.caret_moved())
      << "Caret was moved even though the contenteditable element was already "
         "the active element. This means ExecuteSendKeysToElement() did not "
         "correctly detect the focused state, likely falling back to "
         "IsElementFocused() which gates on document.hasFocus().";
}
