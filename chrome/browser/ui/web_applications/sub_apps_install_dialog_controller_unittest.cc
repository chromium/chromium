// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

using SubAppsInstallDialogControllerTest = BrowserWithTestWindowTest;

constexpr const char kParentAppName[] = "Parent App";
constexpr const char kParentAppScope[] = "https://www.parent-app.com/";

std::unique_ptr<SubAppsInstallDialogController> CreateDefaultController(
    base::OnceCallback<void(bool)> callback,
    gfx::NativeWindow window) {
  auto controller = std::make_unique<SubAppsInstallDialogController>();
  controller->Init(std::move(callback), {}, kParentAppName, kParentAppScope,
                   window);
  return controller;
}

TEST_F(SubAppsInstallDialogControllerTest, DialogViewSetUpCorrectly) {
  std::vector<SubAppDialogInfo> sub_apps = {
      {u"Sub App 1"}, {u"Sub App 2"}, {u"Sub App 3"}};

  views::Widget* widget = chrome::CreateSubAppsInstallDialogWidget(
      kParentAppName, kParentAppScope, sub_apps, GetContext());
  views::DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate();

  EXPECT_FALSE(dialog->ShouldShowCloseButton());
  EXPECT_TRUE(dialog->owned_by_widget());

  // Confirm that all sub app names were added to the view.
  std::vector<views::View*> views_group;
  widget->GetContentsView()->GetViewsInGroup(
      base::to_underlying(SubAppsInstallDialogController::
                              DialogViewIDForTesting::SUB_APP_LABEL),
      &views_group);
  EXPECT_EQ(views_group.size(), 3u);
  widget->CloseNow();
}

TEST_F(SubAppsInstallDialogControllerTest, SubAppConvertedCorrectly) {
  std::u16string sub_app_name = u"Sub App 1";

  auto sub_app_install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://www.app.com"));
  sub_app_install_info->title = sub_app_name;

  std::vector<std::unique_ptr<WebAppInstallInfo>> sub_apps;
  sub_apps.emplace_back(std::move(sub_app_install_info));

  auto controller = std::make_unique<SubAppsInstallDialogController>();
  controller->Init(base::DoNothing(), sub_apps, kParentAppName, kParentAppScope,
                   GetContext());
  views::Widget* widget = controller->GetWidgetForTesting();

  std::vector<views::View*> views_group;
  widget->GetContentsView()->GetViewsInGroup(
      base::to_underlying(SubAppsInstallDialogController::
                              DialogViewIDForTesting::SUB_APP_LABEL),
      &views_group);

  EXPECT_EQ(views_group.size(), 1u);
  EXPECT_EQ(static_cast<views::Label*>(views_group[0])->GetText(),
            sub_app_name);
}

TEST_F(SubAppsInstallDialogControllerTest, DialogAccepted) {
  base::test::TestFuture<bool> future;
  auto controller = CreateDefaultController(future.GetCallback(), GetContext());
  auto* dialog =
      controller->GetWidgetForTesting()->widget_delegate()->AsDialogDelegate();

  dialog->AcceptDialog();

  EXPECT_TRUE(future.Get());
}

TEST_F(SubAppsInstallDialogControllerTest, DialogCancelled) {
  base::test::TestFuture<bool> future;
  auto controller = CreateDefaultController(future.GetCallback(), GetContext());
  auto* dialog =
      controller->GetWidgetForTesting()->widget_delegate()->AsDialogDelegate();

  dialog->CancelDialog();

  EXPECT_FALSE(future.Get());
}

TEST_F(SubAppsInstallDialogControllerTest, EscPressed) {
  base::test::TestFuture<bool> future;
  auto controller = CreateDefaultController(future.GetCallback(), GetContext());
  views::Widget* widget = controller->GetWidgetForTesting();

  // Simulate esc key press.
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  widget->OnKeyEvent(&event);

  EXPECT_FALSE(future.Get());
}

TEST_F(SubAppsInstallDialogControllerTest,
       WidgetLifetimeIsTiedToControllerLifetime) {
  views::Widget* widget;

  {
    auto controller = CreateDefaultController(base::DoNothing(), GetContext());
    widget = controller->GetWidgetForTesting();
    EXPECT_TRUE(widget->IsVisible());
  }

  views::test::WidgetDestroyedWaiter widget_observer(widget);
  widget_observer.Wait();
}

}  // namespace web_app
