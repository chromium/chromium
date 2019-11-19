// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accelerator_table.h"

#include <stddef.h>

#include <set>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

#if defined(OS_CHROMEOS)
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
  std::set<AcceleratorMapping, Cmp> accelerators;
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (auto it = accelerator_list.begin(); it != accelerator_list.end(); ++it) {
    const AcceleratorMapping& entry = *it;
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }
}

#if defined(OS_CHROMEOS)
TEST(AcceleratorTableTest, CheckDuplicatedAcceleratorsAsh) {
  std::set<AcceleratorMapping, Cmp> accelerators;
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (std::vector<AcceleratorMapping>::const_iterator it =
           accelerator_list.begin(); it != accelerator_list.end(); ++it) {
    const AcceleratorMapping& entry = *it;
    accelerators.insert(entry);
  }
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& ash_entry = ash::kAcceleratorData[i];
    if (!ash_entry.trigger_on_press)
      continue;  // kAcceleratorMap does not have any release accelerators.
    // A few shortcuts are defined in the browser as well as in ash so that web
    // contents can consume them. http://crbug.com/309915, 370019, 412435,
    // 321568.
    if (ash_entry.action == ash::WINDOW_MINIMIZE ||
        ash_entry.action == ash::SHOW_TASK_MANAGER ||
        ash_entry.action == ash::OPEN_GET_HELP ||
        ash_entry.action == ash::MINIMIZE_TOP_WINDOW_ON_BACK)
      continue;

    // The following actions are duplicated in both ash and browser accelerator
    // list to ensure BrowserView can retrieve browser command id from the
    // accelerator without needing to know ash.
    // See http://crbug.com/737307 for details.
    if (ash_entry.action == ash::NEW_WINDOW ||
        ash_entry.action == ash::NEW_INCOGNITO_WINDOW ||
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        ash_entry.action == ash::OPEN_FEEDBACK_PAGE ||
#endif
        ash_entry.action == ash::RESTORE_TAB ||
        ash_entry.action == ash::NEW_TAB) {
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
        << (entry.modifiers & ui::EF_ALT_DOWN) << ", action "
        << (ash_entry.action);
  }
}
#endif  // OS_CHROMEOS

}  // namespace chrome
