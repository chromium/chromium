// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/cast_device_footer_view.h"

#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"

using testing::NiceMock;

namespace {

const char kDeviceName[] = "Test Device";

}  // anonymous namespace

class CastDeviceFooterViewTest : public ChromeViewsTestBase {
 public:
  CastDeviceFooterViewTest() = default;
  ~CastDeviceFooterViewTest() override = default;

  void CreateView(std::optional<std::string> device_name) {
    view_ = std::make_unique<CastDeviceFooterView>(
        device_name, mock_stop_casting_callback_.Get(),
        media_message_center::MediaColorTheme());
  }

  CastDeviceFooterView* view() { return view_.get(); }
  views::Button* stop_casting_button() {
    return view_->GetStopCastingButtonForTesting();
  }
  base::MockRepeatingClosure* mock_stop_casting_callback() {
    return &mock_stop_casting_callback_;
  }

 private:
  NiceMock<base::MockRepeatingClosure> mock_stop_casting_callback_;
  std::unique_ptr<CastDeviceFooterView> view_;
};

TEST_F(CastDeviceFooterViewTest, CheckDeviceName) {
  CreateView(kDeviceName);
  EXPECT_EQ(kDeviceName,
            base::UTF16ToUTF8(view()->GetDeviceNameForTesting()->GetText()));
  CreateView(std::nullopt);
  EXPECT_EQ(u"Unknown device", view()->GetDeviceNameForTesting()->GetText());
}

TEST_F(CastDeviceFooterViewTest, ClickingOnStopCastingButton) {
  CreateView(kDeviceName);
  EXPECT_TRUE(stop_casting_button());
  EXPECT_TRUE(stop_casting_button()->GetEnabled());

  EXPECT_CALL(*mock_stop_casting_callback(), Run());
  views::test::ButtonTestApi(stop_casting_button())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(stop_casting_button()->GetEnabled());
}
