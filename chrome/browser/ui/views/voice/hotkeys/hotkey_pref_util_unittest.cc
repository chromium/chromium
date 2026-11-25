// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/hotkeys/hotkey_pref_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace voice {
namespace {

TEST(HotkeyPrefUtilTest, EmptyPrefIsEmpty) {
  EXPECT_TRUE(ParseHotkeyPref("").IsEmpty());
}

TEST(HotkeyPrefUtilTest, UnparsablePrefIsEmpty) {
  EXPECT_TRUE(ParseHotkeyPref("NotAShortcut").IsEmpty());
}

TEST(HotkeyPrefUtilTest, ModifierlessPrefIsEmpty) {
  EXPECT_TRUE(ParseHotkeyPref("D").IsEmpty());
}

TEST(HotkeyPrefUtilTest, ValidPrefIsParsed) {
  const ui::Accelerator accelerator(ui::VKEY_D,
                                    ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ParseHotkeyPref(ui::Command::AcceleratorToString(accelerator)),
            accelerator);
}

}  // namespace
}  // namespace voice
