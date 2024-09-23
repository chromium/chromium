// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_legacy_cast_footer_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"

using testing::NiceMock;

namespace {

// A mock class for handling stop casting button.
class StopCastingHandler {
 public:
  StopCastingHandler() = default;
  ~StopCastingHandler() = default;

  MOCK_METHOD(void, StopCasting, (), ());
};

}  // namespace

class MediaItemUIFooterLegacyCastViewTest : public ChromeViewsTestBase {
 public:
  MediaItemUIFooterLegacyCastViewTest() = default;
  ~MediaItemUIFooterLegacyCastViewTest() override = default;

  void CreateView() {
    handler_ = std::make_unique<StopCastingHandler>();
    base::RepeatingClosure stop_casting_callback = base::BindRepeating(
        &StopCastingHandler::StopCasting, base::Unretained(handler_.get()));
    view_ = std::make_unique<MediaItemUILegacyCastFooterView>(
        stop_casting_callback);
  }

  void SimulateButtonClicked(views::View* view) {
    views::test::ButtonTestApi(static_cast<views::Button*>(view))
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

  MediaItemUILegacyCastFooterView* get_view() { return view_.get(); }
  views::Button* get_stop_casting_button() {
    return view_->GetStopCastingButtonForTesting();
  }
  StopCastingHandler* stop_casting_handler() { return handler_.get(); }

 private:
  std::unique_ptr<StopCastingHandler> handler_;
  std::unique_ptr<MediaItemUILegacyCastFooterView> view_;
};

TEST_F(MediaItemUIFooterLegacyCastViewTest, ClickingOnStopCastingButton) {
  CreateView();
  auto* stop_casting_button = get_stop_casting_button();
  EXPECT_TRUE(stop_casting_button);
  EXPECT_TRUE(stop_casting_button->GetEnabled());

  EXPECT_CALL(*stop_casting_handler(), StopCasting());
  SimulateButtonClicked(stop_casting_button);
  EXPECT_FALSE(stop_casting_button->GetEnabled());
}
