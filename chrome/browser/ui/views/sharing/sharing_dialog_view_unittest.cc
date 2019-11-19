// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync_device_info/device_info.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class SharingDialogViewFake : public SharingDialogView {
 public:
  SharingDialogViewFake(views::View* anchor_view,
                        content::WebContents* web_contents,
                        SharingDialogData data)
      : SharingDialogView(anchor_view, web_contents, std::move(data)) {}
  ~SharingDialogViewFake() override = default;

  // The delegate cannot find widget since it is created from a null profile.
  // This method will be called inside ButtonPressed(). Unit tests will
  // crash without mocking.
  void CloseBubble() override {}
};

}  // namespace

MATCHER_P(DeviceEquals, device, "") {
  return device->guid() == arg.guid();
}

MATCHER_P(AppEquals, app, "") {
  return app->name == arg.name;
}

class SharingDialogViewTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // We create |web_contents| to have a valid commited page origin to check
    // against when showing the origin view.
    GURL url("https://google.com");
    web_contents_ = browser()->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
    CommitPendingLoad(&web_contents_->GetController());
  }

  std::vector<std::unique_ptr<syncer::DeviceInfo>> CreateDevices(int count) {
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;
    for (int i = 0; i < count; i++) {
      devices.emplace_back(std::make_unique<syncer::DeviceInfo>(
          base::StrCat({"device_guid_", base::NumberToString(i)}),
          base::StrCat({"device", base::NumberToString(i)}), "chrome_version",
          "user_agent", sync_pb::SyncEnums_DeviceType_TYPE_PHONE, "device_id",
          base::SysInfo::HardwareInfo(),
          /*last_updated_timestamp=*/base::Time::Now(),
          /*send_tab_to_self_receiving_enabled=*/false,
          /*sharing_info=*/base::nullopt));
    }
    return devices;
  }

  std::vector<SharingApp> CreateApps(int count) {
    std::vector<SharingApp> apps;
    for (int i = 0; i < count; i++) {
      apps.emplace_back(
          &vector_icons::kOpenInNewIcon, gfx::Image(),
          base::UTF8ToUTF16(base::StrCat({"app", base::NumberToString(i)})),
          base::StrCat({"app_id_", base::NumberToString(i)}));
    }
    return apps;
  }

  std::unique_ptr<SharingDialogView> CreateDialogView(
      SharingDialogData dialog_data) {
    auto dialog = std::make_unique<SharingDialogViewFake>(
        /*anchor_view=*/nullptr, web_contents_, std::move(dialog_data));
    dialog->Init();
    return dialog;
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
    data.help_link_text_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TROUBLESHOOT_LINK;
    data.origin_text_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN;

    data.help_callback = base::BindLambdaForTesting(
        [&](SharingDialogType type) { help_callback_.Call(type); });
    data.device_callback =
        base::BindLambdaForTesting([&](const syncer::DeviceInfo& device) {
          device_callback_.Call(device);
        });
    data.app_callback = base::BindLambdaForTesting(
        [&](const SharingApp& app) { app_callback_.Call(app); });

    return data;
  }

  testing::MockFunction<void(SharingDialogType)> help_callback_;
  testing::MockFunction<void(const syncer::DeviceInfo&)> device_callback_;
  testing::MockFunction<void(const SharingApp&)> app_callback_;
  content::WebContents* web_contents_ = nullptr;
};

TEST_F(SharingDialogViewTest, PopulateDialogView) {
  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  auto dialog = CreateDialogView(std::move(dialog_data));

  EXPECT_EQ(5UL, dialog->dialog_buttons_.size());
}

TEST_F(SharingDialogViewTest, DevicePressed) {
  syncer::DeviceInfo device_info("device_guid_1", "device1", "chrome_version",
                                 "user_agent",
                                 sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                                 "device_id", base::SysInfo::HardwareInfo(),
                                 /*last_updated_timestamp=*/base::Time::Now(),
                                 /*send_tab_to_self_receiving_enabled=*/false,
                                 /*sharing_info=*/base::nullopt);
  EXPECT_CALL(device_callback_, Call(DeviceEquals(&device_info)));

  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  auto dialog = CreateDialogView(std::move(dialog_data));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose second device: device0(tag=0), device1(tag=1)
  dialog->ButtonPressed(dialog->dialog_buttons_[1], event);
}

TEST_F(SharingDialogViewTest, AppPressed) {
  SharingApp app(&vector_icons::kOpenInNewIcon, gfx::Image(),
                 base::UTF8ToUTF16("app0"), std::string());
  EXPECT_CALL(app_callback_, Call(AppEquals(&app)));

  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  auto dialog = CreateDialogView(std::move(dialog_data));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose first app: device0(tag=0), device1(tag=1), device2(tag=2),
  // app0(tag=3)
  dialog->ButtonPressed(dialog->dialog_buttons_[3], event);
}

TEST_F(SharingDialogViewTest, HelpTextClickedEmpty) {
  EXPECT_CALL(help_callback_, Call(SharingDialogType::kEducationalDialog));

  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/0);
  auto dialog = CreateDialogView(std::move(dialog_data));

  dialog->StyledLabelLinkClicked(/*label=*/nullptr, /*range=*/{},
                                 /*event_flags=*/0);
}

TEST_F(SharingDialogViewTest, HelpTextClickedOnlyApps) {
  EXPECT_CALL(help_callback_,
              Call(SharingDialogType::kDialogWithoutDevicesWithApp));

  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  auto dialog = CreateDialogView(std::move(dialog_data));

  dialog->StyledLabelLinkClicked(/*label=*/nullptr, /*range=*/{},
                                 /*event_flags=*/0);
}

TEST_F(SharingDialogViewTest, ThemeChangedEmptyList) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.type = SharingDialogType::kErrorDialog;
  auto dialog = CreateDialogView(std::move(dialog_data));

  EXPECT_EQ(SharingDialogType::kErrorDialog, dialog->GetDialogType());

  // Regression test for crbug.com/1001112
  dialog->OnThemeChanged();
}

TEST_F(SharingDialogViewTest, OriginView) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  auto dialog = CreateDialogView(std::move(dialog_data));
  // No footnote by default if there is no initiating origin set.
  EXPECT_EQ(nullptr, dialog->CreateFootnoteView());

  dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://example.com"));
  dialog = CreateDialogView(std::move(dialog_data));
  // Origin should be shown in the footnote if the initiating origin does not
  // match the main frame origin.
  EXPECT_NE(nullptr, dialog->CreateFootnoteView());

  dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://google.com"));
  dialog = CreateDialogView(std::move(dialog_data));
  // Origin should not be shown in the footnote if the initiating origin does
  // match the main frame origin.
  EXPECT_EQ(nullptr, dialog->CreateFootnoteView());
}

TEST_F(SharingDialogViewTest, HelpTextContent) {
  url::Origin current_origin = url::Origin::Create(GURL("https://google.com"));
  url::Origin other_origin = url::Origin::Create(GURL("https://example.com"));
  base::string16 link_text = l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TROUBLESHOOT_LINK);
  base::string16 origin_text = url_formatter::FormatOriginForSecurityDisplay(
      other_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  base::string16 expected_default = l10n_util::GetStringFUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES, link_text);
  base::string16 expected_origin = l10n_util::GetStringFUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES_ORIGIN,
      origin_text, link_text);

  // Expect default help text if no initiating origin is set.
  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  auto dialog = CreateDialogView(std::move(dialog_data));
  auto footnote_view = dialog->CreateFootnoteView();
  EXPECT_EQ(expected_default,
            static_cast<views::StyledLabel*>(footnote_view.get())->GetText());

  // Still expect the default help text if the initiating origin matches the
  // main frame origin.
  dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  dialog_data.initiating_origin = current_origin;
  dialog = CreateDialogView(std::move(dialog_data));
  footnote_view = dialog->CreateFootnoteView();
  EXPECT_EQ(expected_default,
            static_cast<views::StyledLabel*>(footnote_view.get())->GetText());

  // Expect the origin to be included in the help text if it does not match the
  // main frame origin.
  dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  dialog_data.initiating_origin = other_origin;
  dialog = CreateDialogView(std::move(dialog_data));
  footnote_view = dialog->CreateFootnoteView();
  EXPECT_EQ(expected_origin,
            static_cast<views::StyledLabel*>(footnote_view.get())->GetText());
}
