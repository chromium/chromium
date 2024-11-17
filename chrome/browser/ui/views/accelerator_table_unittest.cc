// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/accelerator_table.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/accelerators/accelerator_table.h"
#include "ash/public/cpp/accelerators.h"
#endif

namespace chrome {

namespace {

struct Cmp {
  bool operator()(const AcceleratorMapping& lhs,
                  const AcceleratorMapping& rhs) const {
    if (lhs.keycode != rhs.keycode)
      return lhs.keycode < rhs.keycode;
    return lhs.modifiers < rhs.modifiers;
    // Do not check |command_id|.
  }
};

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  base::flat_set<AcceleratorMapping, Cmp> accelerators;
  for (const auto& entry : GetAcceleratorList()) {
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALTGR_DOWN);
  }
}

TEST(AcceleratorTableTest, PrintKeySupport) {
  int command_id = -1;
  for (const auto& entry : GetAcceleratorList()) {
    if (entry.keycode == ui::VKEY_PRINT) {
      command_id = entry.command_id;
    }
  }
// KEY_PRINT->DomCode::PRINT->VKEY_PRINT are only mapped to IDC_PRINT on
// Chrome OS. On Linux KEY_PRINT is treated as print screen which isn't
// handled by the browser.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(IDC_PRINT, command_id);
#else   // !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(-1, command_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(AcceleratorTableTest, OpenFeedbackWithSearchBasedAccelerator) {
  int command_id = -1;
  for (const auto& entry : GetAcceleratorList()) {
    if (entry.keycode == ui::VKEY_I &&
        entry.modifiers == (ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)) {
      command_id = entry.command_id;
    }
  }

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(IDC_FEEDBACK, command_id);
#else   // !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(-1, command_id);
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(AcceleratorTableTest, CheckDuplicatedAcceleratorsAsh) {
  base::flat_set<AcceleratorMapping, Cmp> accelerators(GetAcceleratorList());
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& ash_entry = ash::kAcceleratorData[i];
    if (!ash_entry.trigger_on_press)
      continue;  // kAcceleratorMap does not have any release accelerators.
    // A few shortcuts are defined in the browser as well as in ash so that web
    // contents can consume them. http://crbug.com/309915, 370019, 412435,
    // 321568.
    if (base::Contains(base::span<const ash::AcceleratorAction>(
                           ash::kActionsInterceptableByBrowser,
                           ash::kActionsInterceptableByBrowserLength),
                       ash_entry.action)) {
      continue;
    }

    // The following actions are duplicated in both ash and browser accelerator
    // list to ensure BrowserView can retrieve browser command id from the
    // accelerator without needing to know ash.
    // See http://crbug.com/737307 for details.
    if (base::Contains(base::span<const ash::AcceleratorAction>(
                           ash::kActionsDuplicatedWithBrowser,
                           ash::kActionsDuplicatedWithBrowserLength),
                       ash_entry.action)) {
      AcceleratorMapping entry;
      entry.keycode = ash_entry.keycode;
      entry.modifiers = ash_entry.modifiers;
      entry.command_id = 0;  // dummy
      // These accelerators should use the same shortcuts in browser accelerator
      // table and ash accelerator table.
      EXPECT_FALSE(accelerators.insert(entry).second)
          << "Action " << ash_entry.action;
      continue;
    }

    AcceleratorMapping entry;
    entry.keycode = ash_entry.keycode;
    entry.modifiers = ash_entry.modifiers;
    entry.command_id = 0;  // dummy
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALTGR_DOWN) << ", action "
        << (ash_entry.action);
  }
}

TEST(AcceleratorTableTest, DontUseKeysWithUnstablePositions) {
  // Some punctuation keys are problematic on international keyboard
  // layouts and should not be used as shortcuts. Two existing shortcuts
  // do use these keys and are excluded (Page Zoom In/Out), and help also
  // uses this key, however it is overridden on Chrome OS in ash.
  // See crbug.com/1174326 for more information.
  for (const auto& entry : GetAcceleratorList()) {
    if (entry.command_id == IDC_ZOOM_MINUS ||
        entry.command_id == IDC_ZOOM_PLUS ||
        entry.command_id == IDC_HELP_PAGE_VIA_KEYBOARD) {
      continue;
    }

    switch (entry.keycode) {
      case ui::VKEY_OEM_PLUS:
      case ui::VKEY_OEM_MINUS:
      case ui::VKEY_OEM_1:
      case ui::VKEY_OEM_2:
      case ui::VKEY_OEM_3:
      case ui::VKEY_OEM_4:
      case ui::VKEY_OEM_5:
      case ui::VKEY_OEM_6:
      case ui::VKEY_OEM_7:
      case ui::VKEY_OEM_8:
      case ui::VKEY_OEM_COMMA:
      case ui::VKEY_OEM_PERIOD:
        FAIL() << "Accelerator command " << entry.command_id
               << " is using a disallowed punctuation key " << entry.keycode
               << ". Prefer to use alphanumeric keys for new shortcuts.";
      default:
        break;
    }
  }
}

#else

// A test fixture for testing GetAcceleratorList().
class GetAcceleratorListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Make sure that previous tests don't affect this test.
    ClearAcceleratorListForTesting();
  }

  void TearDown() override {
    // Make sure that this test doesn't affect following tests.
    ClearAcceleratorListForTesting();
  }
};



// Verify that the shortcuts for DevTools are enabled.
TEST_F(GetAcceleratorListTest, DevToolsAreEnabled) {
  // Verify there is a mapping that is associated to IDC_DEV_TOOLS_TOGGLE.
  std::vector<AcceleratorMapping> list = GetAcceleratorList();
  auto iter = std::find_if(list.begin(), list.end(), [](auto mapping) {
    return mapping.command_id == IDC_DEV_TOOLS_TOGGLE;
  });
  EXPECT_NE(iter, list.end());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chrome
