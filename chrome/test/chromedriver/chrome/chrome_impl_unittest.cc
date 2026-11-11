// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WindowClient : public StubDevToolsClient {
 public:
  Status SendCommand(const std::string& method,
                     const base::DictValue& params) override {
    EXPECT_EQ("Browser.setWindowBounds", method);
    EXPECT_EQ(1, params.FindInt("windowId"));
    commands.push_back(params.Clone());
    if (command_status != kOk) {
      return Status(command_status);
    }
    const base::DictValue* requested = params.FindDict("bounds");
    if (!requested) {
      return Status(kTestError);
    }
    if (const std::string* state = requested->FindString("windowState");
        state && *state == "normal") {
      // Restoration has different geometry from the initial maximized window.
      bounds = base::test::ParseJsonDict(
          R"({"left": 10, "top": 20, "width": 800, "height": 600,
              "windowState": "normal"})");
    }
    bounds.Merge(requested->Clone());
    return Status(kOk);
  }

  Status SendCommandAndGetResult(const std::string& method,
                                 const base::DictValue& params,
                                 base::DictValue* result) override {
    EXPECT_EQ("Browser.getWindowBounds", method);
    EXPECT_EQ(1, params.FindInt("windowId"));
    result->Set("bounds", bounds.Clone());
    return Status(kOk);
  }

  base::DictValue bounds = base::test::ParseJsonDict(
      R"({"left": 0, "top": 0, "width": 1024, "height": 768,
          "windowState": "normal"})");
  std::vector<base::DictValue> commands;
  StatusCode command_status = kOk;
};

class WindowChrome : public ChromeImpl {
 public:
  explicit WindowChrome(std::unique_ptr<WindowClient> client)
      : ChromeImpl(HeadlessBrowserInfo(),
                   {},
                   std::move(client),
                   {},
                   std::nullopt,
                   "normal",
                   false,
                   false) {}

  std::string GetOperatingSystemName() override { return "linux"; }

  internal::Window window{1, "normal", 0, 0, 1024, 768};
  StatusCode lookup_status = kOk;

 protected:
  Status QuitImpl() override { return Status(kOk); }

  Status GetWindow(const std::string& target_id,
                   internal::Window& result) override {
    EXPECT_EQ("target", target_id);
    result = window;
    return Status(lookup_status);
  }

 private:
  static BrowserInfo HeadlessBrowserInfo() {
    BrowserInfo info;
    info.is_headless_shell = true;
    return info;
  }
};

class ChromeImplWindowTest : public testing::Test {
 public:
  ChromeImplWindowTest() {
    auto client = std::make_unique<WindowClient>();
    client_ = client.get();
    chrome_ = std::make_unique<WindowChrome>(std::move(client));
  }

 protected:
  std::unique_ptr<WindowChrome> chrome_;
  raw_ptr<WindowClient> client_;
};

TEST(WindowBoundsTest, MatchesOnlySuppliedComponents) {
  internal::Window window{1, "normal", 0, 0, 800, 600};
  internal::WindowBounds bounds;
  EXPECT_TRUE(bounds.ToDict().empty());
  EXPECT_TRUE(bounds.Matches(window));

  bounds.left = 0;
  EXPECT_EQ(base::test::ParseJsonDict(R"({"left": 0})"), bounds.ToDict());
  EXPECT_TRUE(bounds.Matches(window));
  ++window.left;
  EXPECT_FALSE(bounds.Matches(window));
  bounds.left.reset();

  bounds.top = 0;
  EXPECT_EQ(base::test::ParseJsonDict(R"({"top": 0})"), bounds.ToDict());
  EXPECT_TRUE(bounds.Matches(window));
  ++window.top;
  EXPECT_FALSE(bounds.Matches(window));
  bounds.top.reset();

  bounds.width = 800;
  EXPECT_EQ(base::test::ParseJsonDict(R"({"width": 800})"), bounds.ToDict());
  EXPECT_TRUE(bounds.Matches(window));
  ++window.width;
  EXPECT_FALSE(bounds.Matches(window));
  bounds.width.reset();

  bounds.height = 600;
  EXPECT_EQ(base::test::ParseJsonDict(R"({"height": 600})"), bounds.ToDict());
  EXPECT_TRUE(bounds.Matches(window));
  ++window.height;
  EXPECT_FALSE(bounds.Matches(window));
  bounds.height.reset();

  bounds.state = "normal";
  EXPECT_EQ(base::test::ParseJsonDict(R"({"windowState": "normal"})"),
            bounds.ToDict());
  EXPECT_TRUE(bounds.Matches(window));
  window.state = "maximized";
  EXPECT_FALSE(bounds.Matches(window));
}

TEST(WindowBoundsTest, MatchesEverySuppliedComponent) {
  internal::Window window{1, "normal", 10, 20, 800, 600};
  internal::WindowBounds bounds;
  bounds.left = window.left;
  bounds.top = window.top;
  bounds.width = window.width;
  bounds.height = window.height;
  EXPECT_EQ(base::test::ParseJsonDict(
                R"({"left": 10, "top": 20, "width": 800, "height": 600})"),
            bounds.ToDict());
  EXPECT_TRUE(bounds.Matches(window));
  for (int internal::Window::* component :
       {&internal::Window::left, &internal::Window::top,
        &internal::Window::width, &internal::Window::height}) {
    ++(window.*component);
    EXPECT_FALSE(bounds.Matches(window));
    --(window.*component);
  }
}

TEST_F(ChromeImplWindowTest, SendsIndependentComponents) {
  const char* cases[][2] = {
      {R"({"x": 0})", R"({"left": 0})"},
      {R"({"y": 0})", R"({"top": 0})"},
      {R"({"width": 700})", R"({"width": 700})"},
      {R"({"height": 500})", R"({"height": 500})"},
      {R"({"width": 0})", R"({"width": 0})"},
      {R"({"height": 0})", R"({"height": 0})"},
      {R"({"x": -10, "width": 700})", R"({"left": -10, "width": 700})"},
      {R"({"y": -20, "width": 700})", R"({"top": -20, "width": 700})"},
      {R"({"x": -10, "height": 500})", R"({"left": -10, "height": 500})"},
      {R"({"y": -20, "height": 500})", R"({"top": -20, "height": 500})"},
      {R"({"x": 10, "y": 20, "width": 700, "height": 500})",
       R"({"left": 10, "top": 20, "width": 700, "height": 500})"},
      {R"({})", R"({})"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test[0]);
    client_->commands.clear();
    EXPECT_EQ(
        kOk,
        chrome_->SetWindowRect("target", base::test::ParseJsonDict(test[0]))
            .code());
    ASSERT_EQ(1u, client_->commands.size());
    const base::DictValue* bounds = client_->commands[0].FindDict("bounds");
    ASSERT_TRUE(bounds);
    EXPECT_EQ(base::test::ParseJsonDict(test[1]), *bounds);
  }
}

TEST_F(ChromeImplWindowTest, RestoresBeforeApplyingPartialBounds) {
  for (const char* request : {R"({"width": 700})", R"({})"}) {
    SCOPED_TRACE(request);
    chrome_->window.state = "maximized";
    client_->commands.clear();
    EXPECT_EQ(
        kOk,
        chrome_->SetWindowRect("target", base::test::ParseJsonDict(request))
            .code());
    ASSERT_EQ(2u, client_->commands.size());
    EXPECT_EQ(base::test::ParseJsonDict(
                  R"({"windowId": 1, "bounds": {"windowState": "normal"}})"),
              client_->commands[0]);
    EXPECT_EQ(base::test::ParseJsonDict(request),
              *client_->commands[1].FindDict("bounds"));
    EXPECT_EQ(600, client_->bounds.FindInt("height"));
    EXPECT_EQ(10, client_->bounds.FindInt("left"));
    EXPECT_EQ(20, client_->bounds.FindInt("top"));
  }
}

TEST_F(ChromeImplWindowTest, PreservesStateOnlyCommands) {
  const std::pair<const char*, Status (ChromeImpl::*)(const std::string&)>
      cases[] = {
          {"maximized", &ChromeImpl::MaximizeWindow},
          {"minimized", &ChromeImpl::MinimizeWindow},
          {"fullscreen", &ChromeImpl::FullScreenWindow},
      };
  for (const auto& [state, command] : cases) {
    SCOPED_TRACE(state);
    client_->commands.clear();
    EXPECT_EQ(kOk, ((*chrome_).*command)("target").code());
    ASSERT_EQ(1u, client_->commands.size());
    base::DictValue expected;
    expected.Set("windowId", 1);
    expected.SetByDottedPath("bounds.windowState", state);
    EXPECT_EQ(expected, client_->commands[0]);
  }
}

TEST_F(ChromeImplWindowTest, PropagatesLookupFailureWithoutMutation) {
  chrome_->lookup_status = kNoSuchWindow;
  EXPECT_EQ(kNoSuchWindow,
            chrome_
                ->SetWindowRect("target",
                                base::test::ParseJsonDict(R"({"width": 700})"))
                .code());
  EXPECT_TRUE(client_->commands.empty());
}

TEST_F(ChromeImplWindowTest, PropagatesBackendRefusal) {
  client_->command_status = kUnsupportedOperation;
  EXPECT_EQ(kUnsupportedOperation,
            chrome_
                ->SetWindowRect("target",
                                base::test::ParseJsonDict(R"({"width": 700})"))
                .code());
  ASSERT_EQ(1u, client_->commands.size());
}

TEST_F(ChromeImplWindowTest, PreservesUnknownCommandCompatibility) {
  client_->command_status = kUnknownCommand;
  EXPECT_EQ(kOk, chrome_
                     ->SetWindowRect("target", base::test::ParseJsonDict(
                                                   R"({"width": 700})"))
                     .code());
  ASSERT_EQ(1u, client_->commands.size());
}

}  // namespace
