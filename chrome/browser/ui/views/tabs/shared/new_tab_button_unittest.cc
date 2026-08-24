// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#endif

namespace shared {
namespace {

class NewTabButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
#if BUILDFLAG(IS_CHROMEOS)
    ASSERT_TRUE(user_manager::UserManager::IsInitialized());
    auto* fake_user_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    const AccountId account_id = AccountId::FromUserEmail("test@example.com");
    fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, /*is_affiliated=*/true, user_manager::UserType::kRegular,
        profile_.get());
    fake_user_manager->LoginUser(account_id);
#endif
    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(testing::Const(*mock_browser_window_interface_), GetProfile())
        .WillByDefault(testing::Return(profile_.get()));

#if BUILDFLAG(IS_CHROMEOS)
    on_task_locked_controller_ =
        std::make_unique<ash::boca::OnTaskLockedController>(
            mock_browser_window_interface_.get());
#endif

    init_state_ = std::make_unique<BrowserInitState>(
        BrowserWindowCreateParams(profile_.get(), /*from_user_gesture=*/true),
        mock_browser_window_interface_->GetUnownedUserDataHost());
    test_window_ = std::make_unique<TestBrowserWindow>();
    ON_CALL(*mock_browser_window_interface_, GetWindow())
        .WillByDefault(testing::Return(test_window_.get()));
    ON_CALL(testing::Const(*mock_browser_window_interface_), GetWindow())
        .WillByDefault(testing::Return(test_window_.get()));

    fullscreen_controller_ =
        std::make_unique<BrowserWindowFullscreenController>(
            *mock_browser_window_interface_);
    window_feature_controller_ = std::make_unique<WindowFeatureController>(
        fullscreen_controller_.get(), /*app_controller=*/nullptr,
        BrowserWindowInterface::Type::TYPE_NORMAL,
        /*is_trusted_source=*/false,
        mock_browser_window_interface_->GetUnownedUserDataHost());

    tab_strip_model_delegate_.SetBrowserWindowInterface(
        mock_browser_window_interface_.get());
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, profile_.get());
    ON_CALL(*mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));
    ON_CALL(testing::Const(*mock_browser_window_interface_), GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));

    browser_actions_ =
        std::make_unique<BrowserActions>(mock_browser_window_interface_.get());
    browser_actions_->InitializeBrowserActions();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_params.context = GetContext();
    widget_->Init(std::move(widget_params));

    button_ = widget_->SetContentsView(
        std::make_unique<NewTabButton>(mock_browser_window_interface_.get(),
                                       /*button_size=*/28, /*icon_size=*/16));
    button_->SetBounds(0, 0, 28, 28);
    widget_->Show();
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    browser_actions_.reset();
    window_feature_controller_.reset();
    fullscreen_controller_.reset();
    init_state_.reset();
#if BUILDFLAG(IS_CHROMEOS)
    on_task_locked_controller_.reset();
#endif
    if (tab_strip_model_) {
      tab_strip_model_->CloseAllTabs();
    }
    tab_strip_model_.reset();
    tab_strip_model_delegate_.SetBrowserWindowInterface(nullptr);
    mock_browser_window_interface_.reset();
    test_window_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::boca::OnTaskLockedController> on_task_locked_controller_;
#endif
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  std::unique_ptr<BrowserInitState> init_state_;
  std::unique_ptr<TestBrowserWindow> test_window_;
  std::unique_ptr<BrowserWindowFullscreenController> fullscreen_controller_;
  std::unique_ptr<WindowFeatureController> window_feature_controller_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<NewTabButton> button_;
};

TEST_F(NewTabButtonTest, TriggerableEventFlags) {
#if BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(button_->GetTriggerableEventFlags() & ui::EF_MIDDLE_MOUSE_BUTTON);
#else
  EXPECT_FALSE(button_->GetTriggerableEventFlags() &
               ui::EF_MIDDLE_MOUSE_BUTTON);
#endif
  EXPECT_TRUE(button_->GetTriggerableEventFlags() & ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(NewTabButtonTest, LeftClickButtonCreatesNewTabAndUpdatesInkDrop) {
  TabStripModel* tab_strip_model = tab_strip_model_.get();
  const int initial_count = tab_strip_model->count();

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button_->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), base::TimeTicks::Now(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  button_->OnMouseReleased(release_event);

  EXPECT_EQ(tab_strip_model->count(), initial_count + 1);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);
}

#if BUILDFLAG(IS_LINUX)
TEST_F(NewTabButtonTest, MiddleClickExecutesCallbackAndUpdatesInkDropOnLinux) {
  bool callback_called = false;
  button_->SetMiddleClickCallbackForTesting(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_MIDDLE_MOUSE_BUTTON,
                             ui::EF_MIDDLE_MOUSE_BUTTON);
  EXPECT_TRUE(button_->OnMousePressed(press_event));
  EXPECT_EQ(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), base::TimeTicks::Now(),
                               ui::EF_MIDDLE_MOUSE_BUTTON,
                               ui::EF_MIDDLE_MOUSE_BUTTON);
  button_->OnMouseReleased(release_event);

  EXPECT_TRUE(callback_called);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);
}
#else
TEST_F(NewTabButtonTest, MiddleClickIgnoredOnNonLinux) {
  bool callback_called = false;
  button_->SetMiddleClickCallbackForTesting(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_MIDDLE_MOUSE_BUTTON,
                             ui::EF_MIDDLE_MOUSE_BUTTON);
  button_->OnMousePressed(press_event);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), base::TimeTicks::Now(),
                               ui::EF_MIDDLE_MOUSE_BUTTON,
                               ui::EF_MIDDLE_MOUSE_BUTTON);
  button_->OnMouseReleased(release_event);

  EXPECT_FALSE(callback_called);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);
}
#endif

}  // namespace
}  // namespace shared
