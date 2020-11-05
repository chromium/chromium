// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"

#include <memory>
#include <utility>

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {

namespace {

class MockHost : public WebUIBubbleView::Host {
 public:
  void ShowUI() override { ++show_ui_called_; }
  void CloseUI() override { ++close_ui_called_; }
  void OnWebViewSizeChanged() override { ++size_changed_called_; }

  int show_ui_called() const { return show_ui_called_; }
  int close_ui_called() const { return close_ui_called_; }
  int size_changed_called() const { return size_changed_called_; }

 private:
  int show_ui_called_ = 0;
  int close_ui_called_ = 0;
  int size_changed_called_ = 0;
};

}  // namespace

namespace test {

class WebUIBubbleViewTest : public ChromeViewsTestBase {
 public:
  WebUIBubbleViewTest() = default;
  WebUIBubbleViewTest(const WebUIBubbleViewTest&) = delete;
  WebUIBubbleViewTest& operator=(const WebUIBubbleViewTest&) = delete;
  ~WebUIBubbleViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    scoped_refptr<content::SiteInstance> instance =
        content::SiteInstance::Create(profile_.get());
    instance->GetProcess()->Init();
    test_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), std::move(instance));

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    widget_->Init(std::move(params));
    auto web_view = std::make_unique<WebUIBubbleView>(profile_.get());

    web_view->SetWebContents(test_contents_.get());
    test_contents_->SetDelegate(web_view.get());

    web_view->set_host(&host_);
    web_view_ = widget_->GetContentsView()->AddChildView(std::move(web_view));
  }
  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  WebUIBubbleView* web_view() { return web_view_; }
  Widget* widget() { return widget_.get(); }
  const MockHost& host() { return host_; }

 private:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> test_contents_;
  views::UniqueWidgetPtr widget_;
  WebUIBubbleView* web_view_ = nullptr;
  MockHost host_;
};

TEST_F(WebUIBubbleViewTest, EscapeKeyClosesWidget) {
  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;

  EXPECT_FALSE(widget()->IsClosed());
  web_view()
      ->GetWebContents()
      ->GetRenderWidgetHostView()
      ->GetRenderWidgetHost()
      ->ForwardKeyboardEvent(event);
  EXPECT_TRUE(widget()->IsClosed());
}

TEST_F(WebUIBubbleViewTest, PreferredSizeChangesNotifiesHost) {
  EXPECT_EQ(0, host().size_changed_called());
  constexpr gfx::Size new_size(10, 10);
  EXPECT_NE(web_view()->GetPreferredSize(), new_size);
  web_view()->SetPreferredSize(new_size);
  EXPECT_EQ(1, host().size_changed_called());
}

TEST_F(WebUIBubbleViewTest, ShowUINotifiesHost) {
  EXPECT_EQ(0, host().show_ui_called());
  web_view()->ShowUI();
  EXPECT_EQ(1, host().show_ui_called());
}

TEST_F(WebUIBubbleViewTest, CloseUINotifiesHost) {
  EXPECT_EQ(0, host().close_ui_called());
  web_view()->CloseUI();
  EXPECT_EQ(1, host().close_ui_called());
}

}  // namespace test
}  // namespace views
