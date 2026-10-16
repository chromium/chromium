// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_default.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "chromecast/app/grit/shell_resources.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents_observer.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "url/gurl.h"

namespace chromecast {
namespace {

using ::testing::Return;

class FakeCastContentWindow : public CastContentWindow {
 public:
  FakeCastContentWindow(mojom::CastWebViewParamsPtr params,
                        bool handles_back_press)
      : CastContentWindow(std::move(params)),
        handles_back_press_(handles_back_press) {}
  ~FakeCastContentWindow() override = default;

  void CreateWindow(mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override {}
  void GrantScreenAccess() override {}
  void RevokeScreenAccess() override {}
  void RequestVisibility(VisibilityPriority visibility_priority) override {}
  void EnableTouchInput(bool enabled) override {}
  bool HandlesBackPress() const override { return handles_back_press_; }

 private:
  const bool handles_back_press_;
};

class SelfRemovingWindowObserver : public CastContentWindow::Observer {
 public:
  explicit SelfRemovingWindowObserver(CastContentWindow* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ~SelfRemovingWindowObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
    }
  }

  void OnVisibilityChange(VisibilityType visibility_type) override {}

  void OnWindowDestroyed() override {
    destroyed_count_++;
    if (window_) {
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

  int destroyed_count() const { return destroyed_count_; }

 private:
  raw_ptr<CastContentWindow> window_;
  int destroyed_count_ = 0;
};

class TestPageStopObserver : public CastWebContentsObserver {
 public:
  explicit TestPageStopObserver(CastWebContents* cast_web_contents) {
    Observe(cast_web_contents);
  }
  ~TestPageStopObserver() override = default;

  void PageStopped(PageState page_state, int error_code) override {
    stopped_ = true;
    error_code_ = error_code;
  }

  bool stopped() const { return stopped_; }
  int error_code() const { return error_code_; }

 private:
  bool stopped_ = false;
  int error_code_ = net::ERR_FAILED;
};

input::NativeWebKeyboardEvent MakeKeyEvent(
    blink::WebInputEvent::Type type,
    int windows_key_code,
    int modifiers = blink::WebInputEvent::kNoModifiers) {
  input::NativeWebKeyboardEvent event(type, modifiers, base::TimeTicks::Now());
  event.windows_key_code = windows_key_code;
  return event;
}

class CastWebViewDefaultTest : public content::RenderViewHostTestHarness {
 protected:
  CastWebViewDefaultTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    initializer_ = std::make_unique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();
    web_service_ = std::make_unique<CastWebService>(browser_context(), nullptr);
  }

  void TearDown() override {
    web_view_.reset();
    web_service_.reset();
    task_environment()->RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
    initializer_.reset();
  }

  void CreateWebView(bool window_handles_back_press = false) {
    mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
    auto window = std::make_unique<FakeCastContentWindow>(
        params->Clone(), window_handles_back_press);
    web_view_ = std::make_unique<CastWebViewDefault>(
        std::move(params), web_service_.get(), browser_context(),
        std::move(window));
  }

  content::WebContentsDelegate* GetDelegate() {
    return web_view_->web_contents()->GetDelegate();
  }

  shell::CastBrowserProcess cast_browser_process_;
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
  std::unique_ptr<CastWebService> web_service_;
  std::unique_ptr<CastWebViewDefault> web_view_;
};

TEST_F(CastWebViewDefaultTest, NonBackKeyIsNotHandled) {
  CreateWebView();
  SelfRemovingWindowObserver window_observer(web_view_->window());

  for (int key_code : {ui::VKEY_RETURN, ui::VKEY_BACK}) {
    auto result = GetDelegate()->PreHandleKeyboardEvent(
        web_view_->web_contents(),
        MakeKeyEvent(blink::WebInputEvent::Type::kRawKeyDown, key_code));

    EXPECT_EQ(result, content::KeyboardEventProcessingResult::NOT_HANDLED);
    EXPECT_EQ(window_observer.destroyed_count(), 0);
    EXPECT_NE(web_view_->window(), nullptr);
  }
}

TEST_F(CastWebViewDefaultTest, BackKeyIsNotInterceptedWhenWindowHandlesBack) {
  ui::MockResourceBundleDelegate delegate;
  ui::ResourceBundle resource_bundle_with_mock_delegate(&delegate);
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper(
      &resource_bundle_with_mock_delegate);
  EXPECT_CALL(delegate, LoadDataResourceString(IDR_CAST_SHELL_BACK_PRESSED_JS))
      .Times(0);

  CreateWebView(/*window_handles_back_press=*/true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_view_->web_contents(), GURL("https://example.com"));
  SelfRemovingWindowObserver window_observer(web_view_->window());
  TestPageStopObserver page_observer(web_view_->cast_web_contents());

  auto result = GetDelegate()->PreHandleKeyboardEvent(
      web_view_->web_contents(),
      MakeKeyEvent(blink::WebInputEvent::Type::kRawKeyDown,
                   ui::VKEY_BROWSER_BACK));
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_EQ(result, content::KeyboardEventProcessingResult::NOT_HANDLED);
  EXPECT_EQ(window_observer.destroyed_count(), 0);
  EXPECT_NE(web_view_->window(), nullptr);
  EXPECT_FALSE(page_observer.stopped());
}

TEST_F(CastWebViewDefaultTest,
       BackKeyUpAndAutoRepeatAreConsumedWithoutClosing) {
  CreateWebView();
  SelfRemovingWindowObserver window_observer(web_view_->window());

  auto key_up_result = GetDelegate()->PreHandleKeyboardEvent(
      web_view_->web_contents(),
      MakeKeyEvent(blink::WebInputEvent::Type::kKeyUp, ui::VKEY_ESCAPE));
  EXPECT_EQ(key_up_result, content::KeyboardEventProcessingResult::HANDLED);
  EXPECT_EQ(window_observer.destroyed_count(), 0);

  auto repeat_result = GetDelegate()->PreHandleKeyboardEvent(
      web_view_->web_contents(),
      MakeKeyEvent(blink::WebInputEvent::Type::kRawKeyDown,
                   ui::VKEY_BROWSER_BACK, blink::WebInputEvent::kIsAutoRepeat));
  EXPECT_EQ(repeat_result, content::KeyboardEventProcessingResult::HANDLED);
  EXPECT_EQ(window_observer.destroyed_count(), 0);
  EXPECT_NE(web_view_->window(), nullptr);
}

TEST_F(CastWebViewDefaultTest,
       UnhandledBackKeyClosesWindowAndNotifiesObserver) {
  for (int key_code : {ui::VKEY_BROWSER_BACK, ui::VKEY_ESCAPE}) {
    CreateWebView();
    SelfRemovingWindowObserver window_observer(web_view_->window());
    TestPageStopObserver page_observer(web_view_->cast_web_contents());

    auto result = GetDelegate()->PreHandleKeyboardEvent(
        web_view_->web_contents(),
        MakeKeyEvent(blink::WebInputEvent::Type::kRawKeyDown, key_code));
    task_environment()->RunUntilIdle();

    EXPECT_EQ(result, content::KeyboardEventProcessingResult::HANDLED);
    EXPECT_EQ(window_observer.destroyed_count(), 1);
    EXPECT_EQ(web_view_->window(), nullptr);
    EXPECT_EQ(web_view_->cast_web_contents()->page_state(), PageState::CLOSED);
    EXPECT_TRUE(page_observer.stopped());
    EXPECT_EQ(page_observer.error_code(), net::OK);
  }
}

TEST_F(CastWebViewDefaultTest,
       CommittedPageLoadsBackPressedScriptResourceAndClosesOnTimeout) {
  ui::MockResourceBundleDelegate delegate;
  ui::ResourceBundle resource_bundle_with_mock_delegate(&delegate);
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper(
      &resource_bundle_with_mock_delegate);

  const unsigned char kScriptData[] = "{ false; };";
  scoped_refptr<base::RefCountedStaticMemory> static_memory(
      new base::RefCountedStaticMemory(kScriptData));
  EXPECT_CALL(delegate, LoadDataResourceString(IDR_CAST_SHELL_BACK_PRESSED_JS))
      .WillOnce(Return(
          std::string(reinterpret_cast<const char*>(static_memory->front()),
                      static_memory->size() - 1)));

  CreateWebView();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_view_->web_contents(), GURL("https://example.com"));
  SelfRemovingWindowObserver window_observer(web_view_->window());
  TestPageStopObserver page_observer(web_view_->cast_web_contents());

  auto result = GetDelegate()->PreHandleKeyboardEvent(
      web_view_->web_contents(),
      MakeKeyEvent(blink::WebInputEvent::Type::kRawKeyDown, ui::VKEY_ESCAPE));
  EXPECT_EQ(result, content::KeyboardEventProcessingResult::HANDLED);

  task_environment()->FastForwardBy(base::Milliseconds(500));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(window_observer.destroyed_count(), 1);
  EXPECT_EQ(web_view_->window(), nullptr);
  EXPECT_EQ(web_view_->cast_web_contents()->page_state(), PageState::CLOSED);
  EXPECT_TRUE(page_observer.stopped());
  EXPECT_EQ(page_observer.error_code(), net::OK);
}

}  // namespace
}  // namespace chromecast
