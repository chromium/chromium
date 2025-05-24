// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_volume_slider_view.h"

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/views_test_base.h"

namespace media_message_center {

namespace {
constexpr gfx::Size kVolumeSliderSize = {50, 20};
constexpr float kScrollVolumeDelta = 0.1f;
constexpr float kKeyVolumeDelta = 0.05f;
}  // namespace

class MediaNotificationVolumeSliderViewTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    volume_slider_ = std::make_unique<MediaNotificationVolumeSliderView>(
        base::BindRepeating(&MediaNotificationVolumeSliderViewTest::SetVolume,
                            base::Unretained(this)));
    widget_->SetContentsView(volume_slider_.get());

    widget_->SetBounds(gfx::Rect(kVolumeSliderSize));
    widget_->Show();
  }

  void TearDown() override {
    volume_slider_.reset();
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  void SimulateMousePressed(const gfx::Point& point) {
    volume_slider_->OnMousePressed(ui::MouseEvent(ui::EventType::kMousePressed,
                                                  point, point,
                                                  ui::EventTimeForNow(), 0, 0));
  }

  void SimulateMouseWheelEvent(const gfx::Vector2d& vector) {
    volume_slider_->OnMouseWheel(ui::MouseWheelEvent(
        vector, gfx::Point(), gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

  void SimulateKeyEvent(const ui::KeyboardCode key_code) {
    volume_slider_->OnKeyPressed(
        ui::KeyEvent(ui::EventType::kKeyPressed, key_code, 0));
  }

  MediaNotificationVolumeSliderView* volume_slider() {
    return volume_slider_.get();
  }

  MOCK_METHOD1(SetVolume, void(float));

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MediaNotificationVolumeSliderView> volume_slider_;
};

TEST_F(MediaNotificationVolumeSliderViewTest, SetVolume) {
  EXPECT_CALL(*this, SetVolume(0.0f));
  SimulateMousePressed(gfx::Point(0, 0));

  EXPECT_CALL(*this, SetVolume(0.5f));
  SimulateMousePressed(gfx::Point(kVolumeSliderSize.width() / 2, 0));

  EXPECT_CALL(*this, SetVolume(1.0f));
  SimulateMousePressed(gfx::Point(kVolumeSliderSize.width(), 0));
}

TEST_F(MediaNotificationVolumeSliderViewTest, UpdateVolumeWithMouseWheel) {
  const float volume = 0.8f;
  volume_slider()->SetVolume(volume);

  EXPECT_CALL(*this, SetVolume(volume + kScrollVolumeDelta));
  SimulateMouseWheelEvent(gfx::Vector2d(0, 1));

  EXPECT_CALL(*this, SetVolume(volume - kScrollVolumeDelta));
  SimulateMouseWheelEvent(gfx::Vector2d(0, -1));

  // Test that volume set should be in between 0 and 1.
  volume_slider()->SetVolume(0.95f);
  EXPECT_CALL(*this, SetVolume(1.0f));
  SimulateMouseWheelEvent(gfx::Vector2d(0, 1));

  volume_slider()->SetVolume(0.05f);
  EXPECT_CALL(*this, SetVolume(0.0f));
  SimulateMouseWheelEvent(gfx::Vector2d(0, -1));
}

TEST_F(MediaNotificationVolumeSliderViewTest, UpdateVolumeWithKey) {
  const float volume = 0.5;
  volume_slider()->SetVolume(volume);

  EXPECT_CALL(*this, SetVolume(volume + kKeyVolumeDelta)).Times(2);
  SimulateKeyEvent(ui::KeyboardCode::VKEY_UP);
  SimulateKeyEvent(ui::KeyboardCode::VKEY_RIGHT);

  EXPECT_CALL(*this, SetVolume(volume - kKeyVolumeDelta)).Times(2);
  SimulateKeyEvent(ui::KeyboardCode::VKEY_DOWN);
  SimulateKeyEvent(ui::KeyboardCode::VKEY_LEFT);
}

}  // namespace media_message_center
