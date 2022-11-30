// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class CastDialogNoSinksViewTest : public ChromeViewsTestBase {
 public:
  CastDialogNoSinksViewTest() = default;

  CastDialogNoSinksViewTest(const CastDialogNoSinksViewTest&) = delete;
  CastDialogNoSinksViewTest& operator=(const CastDialogNoSinksViewTest&) =
      delete;

  ~CastDialogNoSinksViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    no_sinks_view_ = std::make_unique<CastDialogNoSinksView>(nullptr);
  }

 protected:
  bool running() const {
    return no_sinks_view_->timer_for_testing().IsRunning();
  }
  const views::View* get_icon() const {
    return no_sinks_view_->icon_for_testing();
  }
  const std::u16string& get_label_text() const {
    return no_sinks_view_->label_text_for_testing();
  }

 private:
  std::unique_ptr<CastDialogNoSinksView> no_sinks_view_;
};

TEST_F(CastDialogNoSinksViewTest, SwitchViews) {
  // Initially the search timer should be running and the icon and label should
  // indicate we are searching for sinks. Icon should never be null.
  EXPECT_TRUE(running());
  const auto* initial_icon = get_icon();
  auto initial_title = get_label_text();
  EXPECT_NE(initial_icon, nullptr);

  // After |kSearchWaitTime| the search timer should have stopped and the icon
  // and label should have changed to indicate no sinks were found.
  task_environment()->FastForwardBy(
      media_router::CastDialogNoSinksView::kSearchWaitTime);
  EXPECT_FALSE(running());
  EXPECT_NE(initial_icon, get_icon());
  EXPECT_NE(initial_title, get_label_text());
}

}  // namespace media_router
