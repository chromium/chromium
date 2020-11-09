// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/key_event_result_receiver.h"

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace arc {

class KeyEventResultReceiverTest : public testing::Test {
 public:
  KeyEventResultReceiverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        receiver_() {}
  ~KeyEventResultReceiverTest() override = default;

  KeyEventResultReceiver* receiver() { return &receiver_; }

  void ForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  KeyEventResultReceiver receiver_;
};

TEST_F(KeyEventResultReceiverTest, ExpireCallback) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventStoppedPropagation) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE};
  event.StopPropagation();
  receiver()->DispatchKeyEventPostIME(&event);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventConsumedByIME) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{ui::ET_KEY_PRESSED,  ui::VKEY_PROCESSKEY,
                     ui::DomCode::NONE,   ui::EF_IS_SYNTHESIZED,
                     ui::DomKey::PROCESS, ui::EventTimeForNow()};
  receiver()->DispatchKeyEventPostIME(&event);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventNotCharacter) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{ui::ET_KEY_PRESSED,      ui::VKEY_LEFT,
                     ui::DomCode::ARROW_LEFT, ui::EF_NONE,
                     ui::DomKey::ARROW_LEFT,  ui::EventTimeForNow()};
  receiver()->DispatchKeyEventPostIME(&event);

  // A KeyEvent with no character is sent to ARC.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, UnmodifiedEnterAndBackspace) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                     ui::DomCode::ENTER, ui::EF_NONE,
                     ui::DomKey::ENTER,  ui::EventTimeForNow()};
  receiver()->DispatchKeyEventPostIME(&event);

  // An Enter key event without modifiers is sent to ARC.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());

  result.reset();
  auto callback2 =
      base::BindLambdaForTesting([&result](bool res) { result = res; });
  receiver()->SetCallback(std::move(callback2));

  ui::KeyEvent event2{ui::ET_KEY_PRESSED,     ui::VKEY_BACK,
                      ui::DomCode::BACKSPACE, ui::EF_NONE,
                      ui::DomKey::BACKSPACE,  ui::EventTimeForNow()};
  receiver()->DispatchKeyEventPostIME(&event);

  // A Backspace key event without modifiers is sent to ARC as well.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, ControlCharacters) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_CONTROL_DOWN};
  receiver()->DispatchKeyEventPostIME(&event);

  // Ctrl-A should be sent to the proxy IME.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventWithSystemModifier) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_ALT_DOWN};
  receiver()->DispatchKeyEventPostIME(&event);

  // Alt-A should be sent to the proxy IME.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, NormalCharacters) {
  base::Optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback));
  EXPECT_FALSE(result.has_value());

  ui::KeyEvent event{'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE};
  receiver()->DispatchKeyEventPostIME(&event);

  // 'A' key should be sent to the proxy IME.
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

}  // namespace arc
