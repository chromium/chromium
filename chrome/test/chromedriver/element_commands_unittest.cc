// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_commands.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
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

namespace {

// Provides responses sufficient to drive GetElementClickableLocation to
// completion for a plain <div>.
Status RespondForClickableLocation(const std::string& function,
                                   std::unique_ptr<base::Value>* result) {
  if (function.find("elem.tagName") != std::string::npos) {
    *result = std::make_unique<base::Value>("div");
  } else if (function ==
             webdriver::atoms::asString(webdriver::atoms::IS_DISPLAYED)) {
    *result = std::make_unique<base::Value>(true);
  } else if (function == webdriver::atoms::asString(
                             webdriver::atoms::GET_LOCATION_IN_VIEW)) {
    base::DictValue dict;
    dict.Set("x", 5.0);
    dict.Set("y", 5.0);
    *result = std::make_unique<base::Value>(std::move(dict));
  } else if (function == webdriver::atoms::asString(
                             webdriver::atoms::IS_ELEMENT_CLICKABLE)) {
    base::DictValue dict;
    dict.Set("clickable", true);
    *result = std::make_unique<base::Value>(std::move(dict));
  } else {
    base::DictValue dict;
    dict.Set("left", 0.0);
    dict.Set("top", 0.0);
    dict.Set("width", 10.0);
    dict.Set("height", 10.0);
    *result = std::make_unique<base::Value>(std::move(dict));
  }
  return Status(kOk);
}

class ChildContainerWebView;

// A child target whose owning entry is dropped during the first script call
// made against it unless the caller has obtained a holder. This mirrors what
// happens to an out-of-process iframe target whose detach is observed while
// CallFunctionWithTimeout is running and only that inner holder had locked it.
class DetachingChildWebView : public StubWebView {
 public:
  explicit DetachingChildWebView(ChildContainerWebView* parent)
      : StubWebView("child"), parent_(parent) {}
  ~DetachingChildWebView() override = default;

  std::unique_ptr<WebViewHolder> GetHolder() override;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override;

 private:
  class Holder : public WebViewHolder {
   public:
    explicit Holder(int* count) : count_(count) { ++*count_; }
    ~Holder() override { --*count_; }

   private:
    raw_ptr<int> count_;
  };

  raw_ptr<ChildContainerWebView> parent_;
  int hold_count_ = 0;
  bool detach_handled_ = false;
};

// Page-level WebView that owns a child target and exposes it via
// FindContainerForFrame, the way FrameTracker does for an OOPIF.
class ChildContainerWebView : public StubWebView {
 public:
  ChildContainerWebView() : StubWebView("page") {
    child_ = std::make_unique<DetachingChildWebView>(this);
  }
  ~ChildContainerWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    return RespondForClickableLocation(function, result);
  }

  WebView* FindContainerForFrame(const std::string& frame_id) override {
    return child_.get();
  }

  void DropChild() {
    child_dropped_ = true;
    child_.reset();
  }

  bool child_dropped() const { return child_dropped_; }

 private:
  std::unique_ptr<StubWebView> child_;
  bool child_dropped_ = false;
};

std::unique_ptr<WebViewHolder> DetachingChildWebView::GetHolder() {
  return std::make_unique<Holder>(&hold_count_);
}

Status DetachingChildWebView::CallFunction(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    std::unique_ptr<base::Value>* result) {
  Status status = RespondForClickableLocation(function, result);
  if (detach_handled_) {
    return status;
  }
  detach_handled_ = true;
  if (hold_count_ == 0) {
    // No outer holder kept this view alive, so the deferred deletion runs as
    // soon as the inner holder goes out of scope. |this| is freed; do not
    // touch members past this point.
    parent_.ExtractAsDangling()->DropChild();
  }
  return status;
}

}  // namespace

// While clicking an element inside an out-of-process iframe, the containing
// child target may detach mid-command. ExecuteClickElement must keep that
// target alive for as long as it holds a raw pointer to it; otherwise the
// remaining script calls and DispatchMouseEvents would run against a freed
// object.
TEST(ElementCommandsTest, ExecuteClickElementSurvivesContainingTargetDetach) {
  ChildContainerWebView webview;
  std::unique_ptr<base::Value> result_value;
  Status status = CallElementCommand(ExecuteClickElement, &webview,
                                     "3247f4d1-ce70-49e9-9a99-bdc7591e032f",
                                     base::DictValue(), true, &result_value);
  EXPECT_TRUE(status.IsOk()) << status.message();
  EXPECT_FALSE(webview.child_dropped())
      << "The containing child target was destroyed while ExecuteClickElement "
         "still held a raw pointer to it.";
}
