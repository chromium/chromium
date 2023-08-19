// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"

#include <cstddef>

#include "base/hash/md5.h"
#include "base/hash/md5_boringssl.h"
#include "base/strings/stringprintf.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Internal builds add an extra accelerator for the Feedback app.
// The total number of Chrome accelerators (available on Chrome OS).
constexpr int kChromeAcceleratorsTotalNum = 103;
// The hash of Chrome accelerators (available on Chrome OS).
constexpr char kChromeAcceleratorsHash[] = "e9e2edcc3a1a7332c960f7b3a00ee8cc";
#else
// The total number of Chrome accelerators (available on Chrome OS).
constexpr int kChromeAcceleratorsTotalNum = 102;
// The hash of Chrome accelerators (available on Chrome OS).
constexpr char kChromeAcceleratorsHash[] = "253127d58865152a7e549bf2caa24027";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

const char kCommonMessage[] =
    "If you are modifying Chrome OS available shortcuts, please update "
    "kAcceleratorLayouts & following the instruction in "
    "ash/webui/shortcut_customization_ui/backend/"
    "accelerator_layout_table.h and the following value(s) on the "
    "top of this file:\n";

const char* BooleanToString(bool value) {
  return value ? "true" : "false";
}

std::string ModifiersToString(int modifiers) {
  return base::StringPrintf("shift=%s control=%s alt=%s search=%s",
                            BooleanToString(modifiers & ui::EF_SHIFT_DOWN),
                            BooleanToString(modifiers & ui::EF_CONTROL_DOWN),
                            BooleanToString(modifiers & ui::EF_ALT_DOWN),
                            BooleanToString(modifiers & ui::EF_COMMAND_DOWN));
}

struct ChromeAcceleratorMappingCmp {
  bool operator()(const AcceleratorMapping& lhs,
                  const AcceleratorMapping& rhs) {
    return std::tie(lhs.keycode, lhs.modifiers) <
           std::tie(rhs.keycode, rhs.modifiers);
  }
};

std::string ChromeAcceleratorMappingToString(
    const AcceleratorMapping& accelerator) {
  return base::StringPrintf("keycode=%d command_id=%d ", accelerator.keycode,
                            accelerator.command_id) +
         ModifiersToString(accelerator.modifiers);
}

std::string HashChromeAcceleratorMapping(
    const std::vector<AcceleratorMapping>& accelerators) {
  base::MD5Context context;
  base::MD5Init(&context);
  for (const auto& accelerator : accelerators) {
    base::MD5Update(&context, ChromeAcceleratorMappingToString(accelerator));
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

class ChromeAcceleratorMetadataTest : public testing::Test {
 public:
  ChromeAcceleratorMetadataTest() = default;

  ChromeAcceleratorMetadataTest(const ChromeAcceleratorMetadataTest&) = delete;
  ChromeAcceleratorMetadataTest& operator=(
      const ChromeAcceleratorMetadataTest&) = delete;

  ~ChromeAcceleratorMetadataTest() override = default;
};

}  // namespace

// Test that modifying chrome accelerator should update kAcceleratorLayouts.
// 1. If you are adding/deleting/modifying shortcuts, please also
//    add/delete/modify the corresponding item in kAcceleratorLayouts.
// 2. Please update the number and hash value of chrome accelerators on the top
// of this file. The new number and hash value will be provided in the test
// output.
TEST_F(ChromeAcceleratorMetadataTest,
       ModifyChromeAcceleratorShouldUpdateLayout) {
  std::vector<AcceleratorMapping> chrome_accelerators;
  for (const auto& accel_mapping : GetAcceleratorList()) {
    chrome_accelerators.emplace_back(accel_mapping);
  }

  const int chrome_accelerators_number = chrome_accelerators.size();
  EXPECT_EQ(chrome_accelerators_number, kChromeAcceleratorsTotalNum)
      << kCommonMessage
      << "kChromeAcceleratorsTotalNum=" << chrome_accelerators_number << "\n";

  std::stable_sort(chrome_accelerators.begin(), chrome_accelerators.end(),
                   ChromeAcceleratorMappingCmp());
  const std::string chrome_accelerators_hash =
      HashChromeAcceleratorMapping(chrome_accelerators);
  EXPECT_EQ(chrome_accelerators_hash, kChromeAcceleratorsHash)
      << kCommonMessage << "kChromeAcceleratorsHash=\""
      << chrome_accelerators_hash << "\"\n";
}

}  // namespace ash
