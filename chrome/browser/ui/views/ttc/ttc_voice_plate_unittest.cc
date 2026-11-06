// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace ttc {

namespace {

using voice::WaveformMode;
using voice::WaveformView;

class TtcVoicePlateTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_view_ = nullptr;
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TtcVoicePlate> CreateVoicePlate(
      base::RepeatingClosure close_callback = base::DoNothing()) {
    return std::make_unique<TtcVoicePlate>(anchor_view_,
                                           std::move(close_callback));
  }

  template <typename T>
  T* GetView(TtcVoicePlate& voice_plate, ui::ElementIdentifier id) {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
        id, views::ElementTrackerViews::GetContextForView(
                voice_plate.GetContentsView()));
  }

  // The text last announced in `voice_plate`'s widget, or empty if none.
  std::u16string GetAnnouncement(TtcVoicePlate& voice_plate) {
    views::View* announce_view = static_cast<views::internal::RootView*>(
                                     voice_plate.GetWidget()->GetRootView())
                                     ->GetAnnounceViewForTesting();
    if (!announce_view) {
      return std::u16string();
    }
    ui::AXNodeData data;
    announce_view->GetViewAccessibility().GetAccessibleNodeData(&data);
    return data.GetString16Attribute(ax::mojom::StringAttribute::kName);
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::View> anchor_view_ = nullptr;
};

TEST_F(TtcVoicePlateTest, CloseButtonRunsCloseCallback) {
  int close_count = 0;
  auto voice_plate = CreateVoicePlate(
      base::BindLambdaForTesting([&close_count]() { ++close_count; }));
  voice_plate->Show();
  auto* close_button = GetView<views::ImageButton>(
      *voice_plate, TtcVoicePlate::kCloseButtonElementId);
  ASSERT_NE(close_button, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), 0, 0);
  views::test::ButtonTestApi(close_button).NotifyClick(click_event);
  EXPECT_EQ(close_count, 1);
}

TEST_F(TtcVoicePlateTest, AccessibleTitleRoleAndStatusName) {
  auto voice_plate = CreateVoicePlate();
  EXPECT_EQ(voice_plate->GetAccessibleWindowTitle(),
            l10n_util::GetStringUTF16(IDS_TTC_ENTRYPOINT_LABEL));
  EXPECT_EQ(voice_plate->GetAccessibleWindowRole(), ax::mojom::Role::kDialog);

  auto* waveform =
      GetView<WaveformView>(*voice_plate, TtcVoicePlate::kWaveformElementId);
  ASSERT_NE(waveform, nullptr);
  views::ViewAccessibility& waveform_accessibility =
      waveform->GetViewAccessibility();
  EXPECT_EQ(waveform_accessibility.GetCachedRole(), ax::mojom::Role::kStatus);

  voice_plate->SetMode(WaveformMode::kProcessing);
  EXPECT_EQ(waveform_accessibility.GetCachedName(),
            l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_PROCESSING_ACCNAME));

  voice_plate->SetMode(WaveformMode::kLive);
  EXPECT_EQ(waveform_accessibility.GetCachedName(),
            l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_LISTENING_ACCNAME));
}

// On Mac, announcements go to the native accessibility API instead of the root
// view's announce view, so GetAnnouncement() can't observe them.
#if !BUILDFLAG(IS_MAC)
TEST_F(TtcVoicePlateTest, AnnouncesListeningWhenVisible) {
  auto voice_plate = CreateVoicePlate();
  voice_plate->SetMode(WaveformMode::kProcessing);
  voice_plate->Show();

  voice_plate->SetMode(WaveformMode::kLive);
  EXPECT_EQ(GetAnnouncement(*voice_plate),
            l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_LISTENING_ACCNAME));
}

// The plate is recreated in kLive when it moves to another window; that
// shouldn't announce listening again.
TEST_F(TtcVoicePlateTest, DoesNotAnnounceBeforeShow) {
  auto voice_plate = CreateVoicePlate();
  voice_plate->SetMode(WaveformMode::kLive);
  voice_plate->Show();
  EXPECT_EQ(GetAnnouncement(*voice_plate), std::u16string());
}
#endif  // !BUILDFLAG(IS_MAC)

}  // namespace

}  // namespace ttc
