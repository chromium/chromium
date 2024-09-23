// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sharing_message/fake_device_info.h"
#include "components/sharing_message/sharing_app.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/url_formatter/elide_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::Property;

MATCHER_P(AppEquals, app, "") {
  return app->name == arg.name;
}

class SharingDialogViewTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    // We create |web_contents_| to have a valid committed page origin to check
    // against when showing the origin view.
    web_contents_ = browser()->OpenURL(
        content::OpenURLParams(GURL("https://google.com"), content::Referrer(),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    CommitPendingLoad(&web_contents_->GetController());
  }

  void TearDown() override {
    if (dialog_)
      dialog_->GetWidget()->CloseNow();
    TestWithBrowserView::TearDown();
  }

  std::vector<SharingTargetDeviceInfo> CreateDevices(int count) {
    std::vector<SharingTargetDeviceInfo> devices;
    for (int i = 0; i < count; ++i) {
      devices.push_back(SharingTargetDeviceInfo(
          "guid_" + base::NumberToString(i), "name_" + base::NumberToString(i),
          SharingDevicePlatform::kUnknown,
          /*pulse_interval=*/base::TimeDelta(),
          syncer::DeviceInfo::FormFactor::kUnknown,
          /*last_updated_timestamp=*/base::Time()));
    }
    return devices;
  }

  std::vector<SharingApp> CreateApps(int count) {
    std::vector<SharingApp> apps;
    for (int i = 0; i < count; ++i) {
      apps.emplace_back(&kOpenInNewIcon, gfx::Image(),
                        base::UTF8ToUTF16("app" + base::NumberToString(i)),
                        "app_id_" + base::NumberToString(i));
    }
    return apps;
  }

  void CreateDialogView(SharingDialogData dialog_data) {
    dialog_ = new SharingDialogView(browser_view(), web_contents_,
                                    std::move(dialog_data));
    views::BubbleDialogDelegateView::CreateBubble(dialog_);
  }

  SharingDialogData CreateDialogData(int devices, int apps) {
    SharingDialogData data;

    if (devices)
      data.type = SharingDialogType::kDialogWithDevicesMaybeApps;
    else if (apps)
      data.type = SharingDialogType::kDialogWithoutDevicesWithApp;
    else
      data.type = SharingDialogType::kEducationalDialog;

    data.prefix = SharingFeatureName::kClickToCall;
    data.devices = CreateDevices(devices);
    data.apps = CreateApps(apps);

    data.help_text_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES;
    data.help_text_origin_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES_ORIGIN;
    data.origin_text_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN;

    data.device_callback =
        base::BindLambdaForTesting([&](const SharingTargetDeviceInfo& device) {
          device_callback_.Call(device);
        });
    data.app_callback = base::BindLambdaForTesting(
        [&](const SharingApp& app) { app_callback_.Call(app); });

    return data;
  }

  SharingDialogView* dialog() { return dialog_; }

  testing::MockFunction<void(const SharingTargetDeviceInfo&)> device_callback_;
  testing::MockFunction<void(const SharingApp&)> app_callback_;

 private:
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
  raw_ptr<SharingDialogView, DanglingUntriaged> dialog_ = nullptr;
};

TEST_F(SharingDialogViewTest, PopulateDialogView) {
  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  CreateDialogView(std::move(dialog_data));

  EXPECT_EQ(5U, dialog()->button_list_for_testing()->children().size());
}

TEST_F(SharingDialogViewTest, DevicePressed) {
  EXPECT_CALL(device_callback_,
              Call(Property(&SharingTargetDeviceInfo::guid, "guid_1")));

  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  CreateDialogView(std::move(dialog_data));

  // Choose second device: device0(tag=0), device1(tag=1)
  const auto& buttons = dialog()->button_list_for_testing()->children();
  ASSERT_EQ(5U, buttons.size());
  views::test::ButtonTestApi(static_cast<views::Button*>(buttons[1]))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
}

TEST_F(SharingDialogViewTest, AppPressed) {
  SharingApp app(&kOpenInNewIcon, gfx::Image(), u"app0", std::string());
  EXPECT_CALL(app_callback_, Call(AppEquals(&app)));

  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  CreateDialogView(std::move(dialog_data));

  // Choose first app: device0(tag=0), device1(tag=1), device2(tag=2),
  // app0(tag=3)
  const auto& buttons = dialog()->button_list_for_testing()->children();
  ASSERT_EQ(5U, buttons.size());
  views::test::ButtonTestApi(static_cast<views::Button*>(buttons[3]))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
}

TEST_F(SharingDialogViewTest, ThemeChangedEmptyList) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.type = SharingDialogType::kErrorDialog;
  CreateDialogView(std::move(dialog_data));

  EXPECT_EQ(SharingDialogType::kErrorDialog, dialog()->GetDialogType());

  // Regression test for crbug.com/1001112
  dialog()->GetWidget()->ThemeChanged();
}

TEST_F(SharingDialogViewTest, FootnoteNoOrigin) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  CreateDialogView(std::move(dialog_data));
  // No footnote by default if there is no initiating origin set.
  EXPECT_EQ(nullptr, dialog()->GetFootnoteViewForTesting());
}

TEST_F(SharingDialogViewTest, FootnoteCurrentOrigin) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://google.com"));
  CreateDialogView(std::move(dialog_data));
  // No footnote if the initiating origin matches the main frame origin.
  EXPECT_EQ(nullptr, dialog()->GetFootnoteViewForTesting());
}

TEST_F(SharingDialogViewTest, FootnoteOtherOrigin) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://example.com"));
  CreateDialogView(std::move(dialog_data));
  // Origin should be shown in the footnote if the initiating origin does not
  // match the main frame origin.
  EXPECT_NE(nullptr, dialog()->GetFootnoteViewForTesting());
}

TEST_F(SharingDialogViewTest, HelpTextNoOrigin) {
  std::u16string expected_default = l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES);

  // Expect default help text if no initiating origin is set.
  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  CreateDialogView(std::move(dialog_data));
  EXPECT_EQ(expected_default, static_cast<views::StyledLabel*>(
                                  dialog()->GetFootnoteViewForTesting())
                                  ->GetText());
}

TEST_F(SharingDialogViewTest, HelpTextCurrentOrigin) {
  std::u16string expected_default = l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES);

  // Expect default help text if the initiating origin matches the main frame
  // origin.
  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://google.com"));
  CreateDialogView(std::move(dialog_data));
  EXPECT_EQ(expected_default, static_cast<views::StyledLabel*>(
                                  dialog()->GetFootnoteViewForTesting())
                                  ->GetText());
}

TEST_F(SharingDialogViewTest, HelpTextOtherOrigin) {
  url::Origin other_origin = url::Origin::Create(GURL("https://example.com"));
  std::u16string origin_text = url_formatter::FormatOriginForSecurityDisplay(
      other_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  std::u16string expected_origin = l10n_util::GetStringFUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES_ORIGIN,
      origin_text);

  // Expect the origin to be included in the help text if it does not match the
  // main frame origin.
  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  dialog_data.initiating_origin = other_origin;
  CreateDialogView(std::move(dialog_data));
  EXPECT_EQ(expected_origin, static_cast<views::StyledLabel*>(
                                 dialog()->GetFootnoteViewForTesting())
                                 ->GetText());
}
