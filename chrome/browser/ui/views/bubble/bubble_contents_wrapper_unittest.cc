// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace views {

namespace {

class MockHost : public BubbleContentsWrapper::Host {
 public:
  // BubbleContentsWrapper::Host:
  void ShowUI() override { ++show_ui_called_; }
  void CloseUI() override { ++close_ui_called_; }
  void ShowCustomContextMenu(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override {
    ++show_custom_context_menu_called_;
  }
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    ++resize_due_to_auto_resize_called_;
  }

  base::WeakPtr<MockHost> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  int show_ui_called() const { return show_ui_called_; }
  int close_ui_called() const { return close_ui_called_; }
  int show_custom_context_menu_called() const {
    return show_custom_context_menu_called_;
  }
  int resize_due_to_auto_resize_called() const {
    return resize_due_to_auto_resize_called_;
  }

 private:
  int show_ui_called_ = 0;
  int close_ui_called_ = 0;
  int show_custom_context_menu_called_ = 0;
  int resize_due_to_auto_resize_called_ = 0;

  base::WeakPtrFactory<MockHost> weak_ptr_factory_{this};
};

class TestBubbleContentsWrapper : public BubbleContentsWrapper {
 public:
  explicit TestBubbleContentsWrapper(Profile* profile)
      : BubbleContentsWrapper(GURL(""), profile, 0, true, true) {}
  ~TestBubbleContentsWrapper() override = default;

  // BubbleContentsWrapper:
  void ReloadWebContents() override {}
};

}  // namespace

namespace test {

class BubbleContentsWrapperTest : public ChromeViewsTestBase {
 public:
  BubbleContentsWrapperTest() = default;
  BubbleContentsWrapperTest(const BubbleContentsWrapperTest&) = delete;
  BubbleContentsWrapperTest& operator=(const BubbleContentsWrapperTest&) =
      delete;
  ~BubbleContentsWrapperTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    scoped_refptr<content::SiteInstance> instance =
        content::SiteInstance::Create(profile_.get());
    instance->GetProcess()->Init();
    auto test_contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), std::move(instance));

    contents_wrapper_ =
        std::make_unique<TestBubbleContentsWrapper>(profile_.get());
    contents_wrapper_->SetWebContentsForTesting(std::move(test_contents));
  }

  BubbleContentsWrapper* contents_wrapper() { return contents_wrapper_.get(); }

 private:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

TEST_F(BubbleContentsWrapperTest, CallsHostForShowUIAndCloseUIWhenPresent) {
  MockHost host;
  EXPECT_EQ(0, host.show_ui_called());
  EXPECT_EQ(0, host.close_ui_called());

  contents_wrapper()->SetHost(host.GetWeakPtr());
  contents_wrapper()->ShowUI();
  contents_wrapper()->CloseUI();
  EXPECT_EQ(1, host.show_ui_called());
  EXPECT_EQ(1, host.close_ui_called());

  contents_wrapper()->SetHost(nullptr);
  contents_wrapper()->ShowUI();
  contents_wrapper()->CloseUI();
  EXPECT_EQ(1, host.show_ui_called());
  EXPECT_EQ(1, host.close_ui_called());
}

TEST_F(BubbleContentsWrapperTest, CallsShowContextMenu) {
  MockHost host;
  EXPECT_EQ(0, host.show_custom_context_menu_called());

  contents_wrapper()->SetHost(host.GetWeakPtr());
  contents_wrapper()->ShowContextMenu(gfx::Point(0, 0), nullptr);
  EXPECT_EQ(1, host.show_custom_context_menu_called());

  contents_wrapper()->SetHost(nullptr);
  contents_wrapper()->ShowContextMenu(gfx::Point(0, 0), nullptr);
  EXPECT_EQ(1, host.show_custom_context_menu_called());
}

TEST_F(BubbleContentsWrapperTest, NotifiesHostWhenResized) {
  MockHost host;
  EXPECT_EQ(0, host.resize_due_to_auto_resize_called());

  contents_wrapper()->SetHost(host.GetWeakPtr());
  contents_wrapper()->ResizeDueToAutoResize(contents_wrapper()->web_contents(),
                                            gfx::Size());
  EXPECT_EQ(1, host.resize_due_to_auto_resize_called());

  contents_wrapper()->SetHost(nullptr);
  contents_wrapper()->ResizeDueToAutoResize(contents_wrapper()->web_contents(),
                                            gfx::Size());
  EXPECT_EQ(1, host.resize_due_to_auto_resize_called());
}

TEST_F(BubbleContentsWrapperTest, EscapeKeyClosesHost) {
  MockHost host;
  contents_wrapper()->SetHost(host.GetWeakPtr());

  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;

  EXPECT_EQ(0, host.close_ui_called());
  contents_wrapper()
      ->web_contents()
      ->GetRenderWidgetHostView()
      ->GetRenderWidgetHost()
      ->ForwardKeyboardEvent(event);
  EXPECT_EQ(1, host.close_ui_called());
}

TEST_F(BubbleContentsWrapperTest, ClosesHostOnWebContentsCrash) {
  MockHost host;
  contents_wrapper()->SetHost(host.GetWeakPtr());
  EXPECT_EQ(0, host.close_ui_called());

  contents_wrapper()->PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED);

  EXPECT_EQ(1, host.close_ui_called());
}

}  // namespace test
}  // namespace views
