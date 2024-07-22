// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

using DialogViewIDForTesting =
    SubAppsInstallDialogController::SubAppsInstallDialogViewID;

constexpr const char kParentAppName[] = "Parent App";
constexpr const char kParentAppScope[] = "https://www.parent-app.com/";

constexpr const char kSubAppStartURL[] = "https://www.parent-app.com/sub-app";
constexpr const char kSubAppIconURL[] =
    "https://www.parent-app.com/sub-app/icon";

const std::u16string kSubAppName1 = u"Sub App 1";
const std::u16string kSubAppName2 = u"Sub App 2";
const std::u16string kSubAppName3 = u"Sub App 3";

class SubAppsInstallDialogControllerTest : public BrowserWithTestWindowTest {
 protected:
  std::unique_ptr<SubAppsInstallDialogController> CreateDefaultController(
      base::OnceCallback<void(bool)> callback) {
    const webapps::AppId parent_app_id =
        web_app::GenerateAppIdFromManifestId(GURL(kParentAppScope));

    auto controller = std::make_unique<SubAppsInstallDialogController>();
    controller->Init(std::move(callback), /*sub_apps=*/{}, kParentAppName,
                     parent_app_id, GetProfile(), GetContext());
    return controller;
  }
};

std::unique_ptr<WebAppInstallInfo> CreateInstallInfoWithIconForSubApp(
    const std::u16string& name) {
  const GeneratedIconsInfo icon_info(
      IconPurpose::ANY, {web_app::icon_size::k32}, {SK_ColorBLACK});
  std::unique_ptr<WebAppInstallInfo> sub_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(kSubAppStartURL));
  sub_app_install_info->title = name;
  web_app::AddIconsToWebAppInstallInfo(sub_app_install_info.get(),
                                       GURL(kSubAppIconURL), {icon_info});
  return sub_app_install_info;
}

TEST_F(SubAppsInstallDialogControllerTest, DialogViewSetUpCorrectly) {
  std::vector<std::unique_ptr<WebAppInstallInfo>> sub_apps;
  sub_apps.emplace_back(CreateInstallInfoWithIconForSubApp(kSubAppName1));
  sub_apps.emplace_back(CreateInstallInfoWithIconForSubApp(kSubAppName2));
  sub_apps.emplace_back(CreateInstallInfoWithIconForSubApp(kSubAppName3));

  views::Widget* widget = CreateSubAppsInstallDialogWidget(
      base::ASCIIToUTF16(std::string(kParentAppName)), sub_apps,
      base::DoNothing(), GetContext());
  views::DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate();

  EXPECT_FALSE(dialog->ShouldShowCloseButton());
  EXPECT_TRUE(dialog->owned_by_widget());

  // Confirm that all sub app names and icons were added to the view.
  std::vector<raw_ptr<views::View, VectorExperimental>> sub_app_labels;
  widget->GetContentsView()->GetViewsInGroup(
      base::to_underlying(DialogViewIDForTesting::SUB_APP_LABEL),
      &sub_app_labels);
  EXPECT_EQ(sub_app_labels.size(), 3u);

  std::vector<raw_ptr<views::View, VectorExperimental>> sub_app_icons;
  widget->GetContentsView()->GetViewsInGroup(
      base::to_underlying(DialogViewIDForTesting::SUB_APP_ICON),
      &sub_app_icons);
  EXPECT_EQ(sub_app_icons.size(), 3u);

  widget->CloseNow();
}

TEST_F(SubAppsInstallDialogControllerTest, SubAppConvertedCorrectly) {
  std::unique_ptr<WebAppInstallInfo> sub_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(kSubAppStartURL));
  sub_app_install_info->title = kSubAppName1;
  const GeneratedIconsInfo icon_info(
      IconPurpose::ANY, {web_app::icon_size::k32}, {SK_ColorBLACK});
  web_app::AddIconsToWebAppInstallInfo(sub_app_install_info.get(),
                                       GURL(kSubAppIconURL), {icon_info});

  std::vector<std::unique_ptr<WebAppInstallInfo>> sub_apps;
  sub_apps.emplace_back(std::move(sub_app_install_info));

  webapps::AppId parent_app_id =
      web_app::GenerateAppIdFromManifestId(GURL(kParentAppScope));
  auto controller = std::make_unique<SubAppsInstallDialogController>();
  controller->Init(base::DoNothing(), sub_apps, kParentAppName, parent_app_id,
                   GetProfile(), GetContext());
  views::Widget* widget = controller->GetWidgetForTesting();

  std::vector<raw_ptr<views::View, VectorExperimental>> sub_app_labels;
  widget->GetContentsView()->GetViewsInGroup(
      base::to_underlying(DialogViewIDForTesting::SUB_APP_LABEL),
      &sub_app_labels);
  EXPECT_EQ(sub_app_labels.size(), 1u);
  EXPECT_EQ(static_cast<views::Label*>(sub_app_labels[0])->GetText(),
            kSubAppName1);

  std::vector<raw_ptr<views::View, VectorExperimental>> sub_app_icons;
  widget->GetContentsView()->GetViewsInGroup(
      base::to_underlying(DialogViewIDForTesting::SUB_APP_ICON),
      &sub_app_icons);
  EXPECT_EQ(sub_app_icons.size(), 1u);
  views::ImageView* icon_view =
      static_cast<views::ImageView*>(sub_app_icons[0]);
  EXPECT_FALSE(icon_view->GetImageModel().IsEmpty());
  EXPECT_EQ(icon_view->GetVisibleBounds().size(), gfx::Size(32, 32));
}

TEST_F(SubAppsInstallDialogControllerTest, DialogAccepted) {
  base::test::TestFuture<bool> future;
  auto controller = CreateDefaultController(future.GetCallback());
  auto* dialog =
      controller->GetWidgetForTesting()->widget_delegate()->AsDialogDelegate();

  dialog->AcceptDialog();

  EXPECT_TRUE(future.Get());
}

TEST_F(SubAppsInstallDialogControllerTest, DialogCancelled) {
  base::test::TestFuture<bool> future;
  auto controller = CreateDefaultController(future.GetCallback());
  auto* dialog =
      controller->GetWidgetForTesting()->widget_delegate()->AsDialogDelegate();

  dialog->CancelDialog();

  EXPECT_FALSE(future.Get());
}

TEST_F(SubAppsInstallDialogControllerTest, EscPressed) {
  base::test::TestFuture<bool> future;
  auto controller = CreateDefaultController(future.GetCallback());
  views::Widget* widget = controller->GetWidgetForTesting();

  // Simulate esc key press.
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, ui::EF_NONE);
  widget->OnKeyEvent(&event);

  EXPECT_FALSE(future.Get());
}

TEST_F(SubAppsInstallDialogControllerTest,
       WidgetLifetimeIsTiedToControllerLifetime) {
  views::Widget* widget;

  {
    auto controller = CreateDefaultController(base::DoNothing());
    widget = controller->GetWidgetForTesting();
    EXPECT_TRUE(widget->IsVisible());
  }

  views::test::WidgetDestroyedWaiter widget_observer(widget);
  widget_observer.Wait();
}

}  // namespace web_app
