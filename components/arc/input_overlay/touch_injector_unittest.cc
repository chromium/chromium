// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/touch_injector.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/json/json_reader.h"
#include "components/arc/input_overlay/actions/action_tap_key.h"
#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {
namespace {
constexpr const char kValidJsonActionTapKey[] =
    R"json({
      "tap": {
        "keyboard": [
          {
            "name": "Fight",
            "key": "KeyA",
            "location": [
              {
                "type": "position",
                "anchor": [
                  0,
                  0
                ],
                "anchor_to_target": [
                  0.5,
                  0.5
                ]
              },
              {
                "type": "position",
                "anchor": [
                  0,
                  0
                ],
                "anchor_to_target": [
                  0.3,
                  0.3
                ]
              }
            ]
          },
          {
            "name": "Run",
            "key": "KeyB",
            "location": [
              {
                "type": "position",
                "anchor_to_target": [
                  0.8,
                  0.8
                ]
              }
            ]
          }
        ]
      }
    })json";
}  // namespace

// Records all key events for testing.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() = default;
  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;
  ~EventCapturer() override = default;

  void Clear() {
    key_events_.clear();
    touch_events_.clear();
  }

  std::vector<std::unique_ptr<ui::KeyEvent>>& key_events() {
    return key_events_;
  }
  std::vector<std::unique_ptr<ui::TouchEvent>>& touch_events() {
    return touch_events_;
  }

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    key_events_.emplace_back(std::make_unique<ui::KeyEvent>(*event));
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    touch_events_.emplace_back(std::make_unique<ui::TouchEvent>(*event));
  }

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events_;
  std::vector<std::unique_ptr<ui::TouchEvent>> touch_events_;
};

class TouchInjectorTest : public views::ViewsTestBase {
 protected:
  TouchInjectorTest() = default;

  std::unique_ptr<input_overlay::ActionTapKey> CreateActionTapKey(
      base::StringPiece json,
      aura::Window* window) {
    base::JSONReader::ValueWithError json_value =
        base::JSONReader::ReadAndReturnValueWithError(json);
    auto action = std::make_unique<input_overlay::ActionTapKey>(window);
    action->ParseFromJson(json_value.value.value());
    return action;
  }

  std::unique_ptr<views::Widget> CreateArcWindow() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(200, 100, 200, 400);
    params.context = root_window();
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    auto widget = std::make_unique<views::Widget>();
    widget->Init(std::move(params));
    widget->widget_delegate()->SetCanResize(true);
    widget->SetBounds(gfx::Rect(200, 100, 200, 400));
    auto app_id = absl::optional<std::string>("app_id");
    widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, *app_id);
    widget->GetNativeWindow()->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::ARC_APP));
    widget->Show();
    widget->Activate();

    return widget;
  }

  aura::TestScreen* test_screen() {
    return aura::test::AuraTestHelper::GetInstance()->GetTestScreen();
  }

  aura::Window* root_window() { return GetContext(); }

  EventCapturer event_capturer_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

 private:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(root_window());
  }

  void TearDown() override {
    event_generator_.reset();
    event_capturer_.Clear();
    views::ViewsTestBase::TearDown();
  }
};

TEST_F(TouchInjectorTest, TestEventRewriterActionTapKey) {
  test_screen()->SetDeviceScaleFactor(1.5f);
  root_window()->SetBounds(gfx::Rect(800, 600));

  auto widget = CreateArcWindow();
  int height = -widget->non_client_view()
                    ->frame_view()
                    ->GetWindowBoundsForClientBounds(gfx::Rect())
                    .y();

  root_window()->AddPostTargetHandler(&event_capturer_);

  auto injector = std::make_unique<TouchInjector>(widget->GetNativeWindow());
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector->ParseActions(json_value.value.value());
  auto* actionA = injector->actions()[0].get();
  auto* actionB = injector->actions()[1].get();
  injector->RegisterEventRewriter();

  // Press and release key A, it should receive touch event, not key event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_TRUE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expectA1 = gfx::PointF(300, 300);
  expectA1.Offset(0, height);
  EXPECT_EQ(expectA1, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_FALSE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(expectA1, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);
  // Next touch position.
  EXPECT_EQ(1, actionA->current_position_index());
  // Unregister the event rewriter to see if extra events are sent.
  injector->UnRegisterEventRewriter();

  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event_capturer_.Clear();
  injector->RegisterEventRewriter();

  // Press and release key C, it should receive key event, not touch event.
  event_generator_->PressKey(ui::VKEY_C, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.key_events().size());
  event_generator_->ReleaseKey(ui::VKEY_C, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.key_events().size());
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release A -> Release B.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expectB = gfx::PointF(360, 420);
  expectB.Offset(0, height);
  EXPECT_EQ(expectB, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expectA2 = gfx::PointF(260, 220);
  expectA2.Offset(0, height);
  EXPECT_EQ(expectA2, event->root_location_f());
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(expectA2, event->root_location_f());
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(expectB, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release B -> Release A.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_EQ(expectB, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_EQ(expectA1, event->root_location_f());
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(expectB, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(expectA1, event->root_location_f());
  EXPECT_EQ(1, event->pointer_details().id);
  event_capturer_.Clear();

  // Test repeat key and it should receive only one touch event.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_generator_->PressKey(ui::VKEY_B, ui::EF_IS_REPEAT, 1);
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(expectB, event->root_location_f());
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_capturer_.Clear();

  // Test cancel key.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(actionB->touch_id());
  injector->UnRegisterEventRewriter();
  EXPECT_FALSE(actionB->touch_id());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events().back().get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_CANCELLED, event->type());
  EXPECT_EQ(expectB, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();
  // Register the event-rewriter and press key again.
  injector->RegisterEventRewriter();
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(actionB->touch_id());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_EQ(expectB, event->root_location_f());
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_capturer_.Clear();

  root_window()->RemovePreTargetHandler(&event_capturer_);
  widget->CloseNow();
}

}  // namespace arc
