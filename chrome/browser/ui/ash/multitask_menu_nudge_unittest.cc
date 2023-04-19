// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/wm/features.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/aura/window.h"

namespace ash {

class MultitaskMenuNudgeTest : public AshTestBase {
 public:
  MultitaskMenuNudgeTest()
      : scoped_feature_list_(chromeos::wm::features::kWindowLayoutMenu) {}
  MultitaskMenuNudgeTest(const MultitaskMenuNudgeTest&) = delete;
  MultitaskMenuNudgeTest& operator=(const MultitaskMenuNudgeTest&) = delete;
  ~MultitaskMenuNudgeTest() override = default;

  views::Widget* GetNudgeWidgetForWindow(aura::Window* window) {
    auto* frame = NonClientFrameViewAsh::Get(window);
    CHECK(frame);
    chromeos::MultitaskMenuNudgeController* nudge_controller =
        chromeos::FrameCaptionButtonContainerView::TestApi(
            frame->GetHeaderView()->caption_button_container())
            .nudge_controller();
    return nudge_controller ? nudge_controller->nudge_widget_.get() : nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MultitaskMenuNudgeTest, NoNudgeForNewUser) {
  chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(false);

  auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
  fake_user_manager->set_is_current_user_new(true);
  user_manager::ScopedUserManager scoped_user_manager(
      std::move(fake_user_manager));

  auto window = CreateAppWindow(gfx::Rect(300, 300));
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

}  // namespace ash
