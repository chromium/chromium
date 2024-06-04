// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/views/web_apps/view_visible_in_active_widget_notifier.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace web_apps {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kRandomLabel);

class ViewVisibleInActiveWidgetNotifierTest : public ChromeViewsTestBase {
 public:
  ViewVisibleInActiveWidgetNotifierTest() = default;
  ~ViewVisibleInActiveWidgetNotifierTest() override = default;

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->SetContentsView(
        views::Builder<views::Label>(
            std::make_unique<views::Label>(u"Random Label"))
            .SetProperty(views::kElementIdentifierKey, kRandomLabel)
            .Build());
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }
  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ViewVisibleInActiveWidgetNotifierTest, WorksBeforeWidgetShown) {
  base::test::TestFuture<bool> future;
  base::WeakPtr<web_app::ViewVisibleInActiveWidgetNotifier> notifier =
      web_app::ViewVisibleInActiveWidgetNotifier::Create(widget(), kRandomLabel,
                                                         future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  widget()->Show();
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
  EXPECT_FALSE(notifier);
}

TEST_F(ViewVisibleInActiveWidgetNotifierTest, WorksAfterWidgetShown) {
  widget()->Show();
  base::test::TestFuture<bool> future;
  base::WeakPtr<web_app::ViewVisibleInActiveWidgetNotifier> notifier =
      web_app::ViewVisibleInActiveWidgetNotifier::Create(widget(), kRandomLabel,
                                                         future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
  EXPECT_FALSE(notifier);
}

TEST_F(ViewVisibleInActiveWidgetNotifierTest, WidgetDestructionNoDangling) {
  base::test::TestFuture<bool> future;
  base::WeakPtr<web_app::ViewVisibleInActiveWidgetNotifier> notifier =
      web_app::ViewVisibleInActiveWidgetNotifier::Create(widget(), kRandomLabel,
                                                         future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  SimulateNativeDestroy(widget());
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(notifier);
}

TEST_F(ViewVisibleInActiveWidgetNotifierTest,
       WorksBeforeWidgetShownOrViewVisible) {
  // Test the view not being visible (or added) when the widget is shown.
  widget()->GetContentsView()->SetVisible(false);

  base::test::TestFuture<bool> future;
  base::WeakPtr<web_app::ViewVisibleInActiveWidgetNotifier> notifier =
      web_app::ViewVisibleInActiveWidgetNotifier::Create(widget(), kRandomLabel,
                                                         future.GetCallback());
  widget()->Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget()->GetContentsView()->GetVisible());
  EXPECT_FALSE(future.IsReady());

  widget()->GetContentsView()->SetVisible(true);
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
  EXPECT_FALSE(notifier);
}

TEST_F(ViewVisibleInActiveWidgetNotifierTest, WorksWhenNotifierWeakPtrRemoved) {
  base::test::TestFuture<bool> future;
  base::WeakPtr<web_app::ViewVisibleInActiveWidgetNotifier> notifier =
      web_app::ViewVisibleInActiveWidgetNotifier::Create(widget(), kRandomLabel,
                                                         future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  notifier.reset();
  EXPECT_FALSE(notifier);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  widget()->Show();
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

}  // namespace

}  // namespace web_apps
