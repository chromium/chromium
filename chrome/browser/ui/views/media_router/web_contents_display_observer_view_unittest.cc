// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/media_router/web_contents_display_observer_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace media_router {

class MockCallback {
 public:
  MOCK_METHOD(void, OnDisplayChanged, (), ());
};

class TestWebContentsDisplayObserverView
    : public WebContentsDisplayObserverView {
 public:
  TestWebContentsDisplayObserverView(content::WebContents* web_contents,
                                     base::RepeatingCallback<void()> callback,
                                     const display::Display& test_display)
      : WebContentsDisplayObserverView(web_contents, std::move(callback)),
        test_display_(test_display) {}

  void set_test_display(const display::Display& test_display) {
    test_display_ = test_display;
  }

 private:
  display::Display GetDisplayNearestWidget() const override {
    return test_display_;
  }

  display::Display test_display_;
};

class WebContentsDisplayObserverViewTest
    : public ChromeRenderViewHostTestHarness {
 public:
  WebContentsDisplayObserverViewTest()
      : ChromeRenderViewHostTestHarness(), display1_(101), display2_(102) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents_ = CreateTestWebContents();
    display_observer_ = std::make_unique<TestWebContentsDisplayObserverView>(
        web_contents_.get(),
        base::BindRepeating(&MockCallback::OnDisplayChanged,
                            base::Unretained(&callback_)),
        display1_);
  }

  void TearDown() override {
    display_observer_.reset();
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestWebContentsDisplayObserverView> display_observer_;
  MockCallback callback_;
  const display::Display display1_;
  const display::Display display2_;
};

TEST_F(WebContentsDisplayObserverViewTest, NotifyWhenDisplayChanges) {
  // Bounds change without display change should not trigger the callback.
  display_observer_->OnWidgetBoundsChanged(nullptr, gfx::Rect(0, 0, 500, 500));

  // If the widget is in a different display after moving, the callback should
  // be called.
  display_observer_->set_test_display(display2_);
  EXPECT_CALL(callback_, OnDisplayChanged());
  display_observer_->OnWidgetBoundsChanged(nullptr,
                                           gfx::Rect(1920, 0, 500, 500));
  EXPECT_EQ(display2_.id(), display_observer_->GetCurrentDisplay().id());
}

}  // namespace media_router
