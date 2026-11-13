// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"

#if !BUILDFLAG(IS_MAC)
#include "base/scoped_observation.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/ax_update_observer.h"
#include "ui/views/view_tracker.h"

namespace {

// Observes `views::AXUpdateNotifier` and invokes `on_alert` once when an
// `ax::mojom::Event::kAlert` accessibility event is dispatched.
class ScopedAlertDestructionObserver : public views::AXUpdateObserver {
 public:
  explicit ScopedAlertDestructionObserver(base::OnceClosure on_alert)
      : on_alert_(std::move(on_alert)) {
    observation_.Observe(views::AXUpdateNotifier::Get());
  }
  ~ScopedAlertDestructionObserver() override = default;

  // views::AXUpdateObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kAlert && on_alert_) {
      observation_.Reset();
      std::move(on_alert_).Run();
    }
  }

 private:
  base::OnceClosure on_alert_;
  base::ScopedObservation<views::AXUpdateNotifier, views::AXUpdateObserver>
      observation_{this};
};

}  // namespace
#endif  // !BUILDFLAG(IS_MAC)

class PasswordChangeToastTest : public ChromeViewsTestBase {
 public:
  PasswordChangeToastTest() = default;
  ~PasswordChangeToastTest() override = default;

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  PasswordChangeToast* ShowToast(PasswordChangeToast::ToastOptions options) {
    auto toast_view = std::make_unique<PasswordChangeToast>(std::move(options));
    auto* raw_toast_view = toast_view.get();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->Show();
    anchor_widget_->SetContentsView(std::move(toast_view));
    return raw_toast_view;
  }

  void DestroyToastWidget() { anchor_widget_.reset(); }

#if !BUILDFLAG(IS_MAC)
  void DestroyToastView(PasswordChangeToast* toast_view) {
    anchor_widget_->GetRootView()->RemoveChildViewT(toast_view);
  }
#endif  // !BUILDFLAG(IS_MAC)

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(PasswordChangeToastTest, DisplayedWithOptions) {
  PasswordChangeToast::ToastOptions options(u"Changing password...",
                                            base::DoNothing());
  auto* toast_view = ShowToast(std::move(options));
  EXPECT_TRUE(toast_view->throbber()->GetVisible());
  EXPECT_FALSE(toast_view->icon_view()->GetVisible());
  EXPECT_TRUE(toast_view->label()->GetVisible());
  EXPECT_EQ(u"Changing password...", toast_view->label()->GetText());
  EXPECT_FALSE(toast_view->action_button()->GetVisible());
  EXPECT_TRUE(toast_view->close_button()->GetVisible());
}

TEST_F(PasswordChangeToastTest, ConfigurationUpdated) {
  PasswordChangeToast::ToastOptions options(u"Checking sign in...",
                                            base::DoNothing());
  auto* toast_view = ShowToast(std::move(options));
  EXPECT_TRUE(toast_view->throbber()->GetVisible());
  EXPECT_FALSE(toast_view->icon_view()->GetVisible());
  EXPECT_TRUE(toast_view->label()->GetVisible());
  EXPECT_EQ(u"Checking sign in...", toast_view->label()->GetText());
  EXPECT_FALSE(toast_view->action_button()->GetVisible());
  EXPECT_TRUE(toast_view->close_button()->GetVisible());

  PasswordChangeToast::ToastOptions new_options(
      u"Password changed",
      features::IsRoundedIconsEnabled() ? vector_icons::kPasswordManagerIcon
                                        : vector_icons::kPasswordManagerOldIcon,
      base::DoNothing(), u"Details", base::DoNothing());
  toast_view->UpdateLayout(std::move(new_options));
  EXPECT_FALSE(toast_view->throbber()->GetVisible());
  EXPECT_TRUE(toast_view->icon_view()->GetVisible());
  EXPECT_TRUE(toast_view->label()->GetVisible());
  EXPECT_EQ(u"Password changed", toast_view->label()->GetText());
  EXPECT_TRUE(toast_view->action_button()->GetVisible());
  EXPECT_EQ(u"Details", toast_view->action_button()->GetText());
  EXPECT_TRUE(toast_view->close_button()->GetVisible());
}

TEST_F(PasswordChangeToastTest, AutoDismissesAfterDelay) {
  base::MockOnceClosure auto_dismiss_callback;
  EXPECT_CALL(auto_dismiss_callback, Run).Times(0);

  PasswordChangeToast::ToastOptions options(
      u"Password changed",
      features::IsRoundedIconsEnabled() ? vector_icons::kPasswordManagerIcon
                                        : vector_icons::kPasswordManagerOldIcon,
      base::DoNothing(), u"Details", base::DoNothing(),
      auto_dismiss_callback.Get());
  ShowToast(std::move(options));

  task_environment()->FastForwardBy(PasswordChangeToast::kAutoDismissDelay -
                                    base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(&auto_dismiss_callback);

  EXPECT_CALL(auto_dismiss_callback, Run);
  task_environment()->FastForwardBy(base::Milliseconds(1));
}

TEST_F(PasswordChangeToastTest, DestroyingToastCancelsAutoDismiss) {
  base::MockOnceClosure auto_dismiss_callback;
  EXPECT_CALL(auto_dismiss_callback, Run).Times(0);

  PasswordChangeToast::ToastOptions options(
      u"Password changed",
      features::IsRoundedIconsEnabled() ? vector_icons::kPasswordManagerIcon
                                        : vector_icons::kPasswordManagerOldIcon,
      base::DoNothing(), u"Details", base::DoNothing(),
      auto_dismiss_callback.Get());
  ShowToast(std::move(options));

  DestroyToastWidget();
  task_environment()->FastForwardBy(PasswordChangeToast::kAutoDismissDelay);
}

#if !BUILDFLAG(IS_MAC)
// On macOS, `RootView::AnnounceTextAs()` dispatches through
// `AXPlatformNode::AnnounceTextAs()` directly rather than firing `kAlert` via
// `AnnounceTextView` and `AXUpdateNotifier`.
TEST_F(PasswordChangeToastTest, UpdateLayoutSurvivesReentrantDestruction) {
  PasswordChangeToast::ToastOptions initial_options(u"Checking sign in...",
                                                    base::DoNothing());
  PasswordChangeToast* toast_view = ShowToast(std::move(initial_options));
  views::ViewTracker toast_tracker(toast_view);

  base::MockOnceClosure auto_dismiss_callback;
  EXPECT_CALL(auto_dismiss_callback, Run).Times(0);

  ScopedAlertDestructionObserver alert_observer(
      base::BindOnce(&PasswordChangeToastTest::DestroyToastView,
                     base::Unretained(this), base::Unretained(toast_view)));

  PasswordChangeToast::ToastOptions new_options(
      u"Password changed",
      features::IsRoundedIconsEnabled() ? vector_icons::kPasswordManagerIcon
                                        : vector_icons::kPasswordManagerOldIcon,
      base::DoNothing(), u"Details", base::DoNothing(),
      auto_dismiss_callback.Get());
  toast_view->UpdateLayout(std::move(new_options));

  EXPECT_EQ(toast_tracker.view(), nullptr);
  task_environment()->FastForwardBy(PasswordChangeToast::kAutoDismissDelay);
}
#endif  // !BUILDFLAG(IS_MAC)
