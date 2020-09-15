// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/mojom/ime.mojom.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace mojo {

namespace {

void ExpectKeyEventsEqual(const ui::KeyEvent& expected,
                          const ui::KeyEvent& actual) {
  EXPECT_EQ(expected.type(), actual.type());
  EXPECT_EQ(expected.key_code(), actual.key_code());
  EXPECT_EQ(expected.code(), actual.code());
  EXPECT_EQ(expected.IsShiftDown(), actual.IsShiftDown());
  EXPECT_EQ(expected.IsAltDown(), actual.IsAltDown());
  EXPECT_EQ(expected.IsControlDown(), actual.IsControlDown());
  EXPECT_EQ(expected.IsCapsLockOn(), actual.IsCapsLockOn());
}

}  // namespace

TEST(KeyEventStructTraitsTest, Convert) {
  const ui::KeyEvent kTestData[] = {
      {ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN},
      {ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, ui::EF_ALT_DOWN},
      {ui::ET_KEY_RELEASED, ui::VKEY_B, ui::DomCode::US_B, ui::EF_SHIFT_DOWN},
      {ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, ui::EF_CAPS_LOCK_ON},
  };
  for (size_t idx = 0; idx < base::size(kTestData); ++idx) {
    auto copy = std::make_unique<ui::KeyEvent>(kTestData[idx]);
    std::unique_ptr<ui::KeyEvent> output;
    mojo::test::SerializeAndDeserialize<arc::mojom::KeyEventData>(&copy,
                                                                  &output);
    ExpectKeyEventsEqual(*copy, *output);
  }
}

}  // namespace mojo
