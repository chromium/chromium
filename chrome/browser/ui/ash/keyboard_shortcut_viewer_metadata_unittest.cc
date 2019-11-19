// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "ash/components/shortcut_viewer/keyboard_shortcut_item.h"
#include "ash/components/strings/grit/ash_components_strings.h"
#include "ash/public/cpp/accelerators.h"
#include "base/hash/md5.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The total number of Ash accelerators.
constexpr int kAshAcceleratorsTotalNum = 110;
// The hash of Ash accelerators.
constexpr char kAshAcceleratorsHash[] = "1287cacd678f63ab151fbf25383ac19c";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Internal builds add an extra accelerator for the Feedback app.
// The total number of Chrome accelerators (available on Chrome OS).
constexpr int kChromeAcceleratorsTotalNum = 93;
// The hash of Chrome accelerators (available on Chrome OS).
constexpr char kChromeAcceleratorsHash[] = "73c842a72d77e7b9e69e92a6ee7900d3";
#else
// The total number of Chrome accelerators (available on Chrome OS).
constexpr int kChromeAcceleratorsTotalNum = 92;
// The hash of Chrome accelerators (available on Chrome OS).
constexpr char kChromeAcceleratorsHash[] = "7c45362e298cf77aae142dec7154adcf";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

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

std::string AshAcceleratorDataToString(
    const ash::AcceleratorData& accelerator) {
  return base::StringPrintf("trigger_on_press=%s keycode=%d action=%d ",
                            BooleanToString(accelerator.trigger_on_press),
                            accelerator.keycode, accelerator.action) +
         ModifiersToString(accelerator.modifiers);
}

std::string ChromeAcceleratorMappingToString(
    const AcceleratorMapping& accelerator) {
  return base::StringPrintf("keycode=%d command_id=%d ", accelerator.keycode,
                            accelerator.command_id) +
         ModifiersToString(accelerator.modifiers);
}

struct AshAcceleratorDataCmp {
  bool operator()(const ash::AcceleratorData& lhs,
                  const ash::AcceleratorData& rhs) {
    return std::tie(lhs.trigger_on_press, lhs.keycode, lhs.modifiers) <
           std::tie(rhs.trigger_on_press, rhs.keycode, rhs.modifiers);
  }
};

struct ChromeAcceleratorMappingCmp {
  bool operator()(const AcceleratorMapping& lhs,
                  const AcceleratorMapping& rhs) {
    return std::tie(lhs.keycode, lhs.modifiers) <
           std::tie(rhs.keycode, rhs.modifiers);
  }
};

std::string HashAshAcceleratorData(
    const std::vector<ash::AcceleratorData>& accelerators) {
  base::MD5Context context;
  base::MD5Init(&context);
  for (const auto& accelerator : accelerators)
    base::MD5Update(&context, AshAcceleratorDataToString(accelerator));

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

std::string HashChromeAcceleratorMapping(
    const std::vector<AcceleratorMapping>& accelerators) {
  base::MD5Context context;
  base::MD5Init(&context);
  for (const auto& accelerator : accelerators)
    base::MD5Update(&context, ChromeAcceleratorMappingToString(accelerator));

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

std::string AcceleratorIdToString(
    const keyboard_shortcut_viewer::AcceleratorId& accelerator_id) {
  return base::StringPrintf("keycode=%d ", accelerator_id.keycode) +
         ModifiersToString(accelerator_id.modifiers);
}

std::string AcceleratorIdsToString(
    const std::vector<keyboard_shortcut_viewer::AcceleratorId>&
        accelerator_ids) {
  std::vector<std::string> msgs;
  for (const auto& id : accelerator_ids)
    msgs.emplace_back(AcceleratorIdToString(id));
  return base::JoinString(msgs, ", ");
}

class KeyboardShortcutViewerMetadataTest : public testing::Test {
 public:
  KeyboardShortcutViewerMetadataTest() = default;
  ~KeyboardShortcutViewerMetadataTest() override = default;

  void SetUp() override {
    for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
      const ash::AcceleratorData& accel_data = ash::kAcceleratorData[i];
      ash_accelerator_ids_.insert({accel_data.keycode, accel_data.modifiers});
    }

    for (const auto& accel_mapping : GetAcceleratorList()) {
      chrome_accelerator_ids_.insert(
          {accel_mapping.keycode, accel_mapping.modifiers});
    }

    testing::Test::SetUp();
  }

 protected:
  // Ash accelerator ids.
  std::set<keyboard_shortcut_viewer::AcceleratorId> ash_accelerator_ids_;

  // Chrome accelerator ids.
  std::set<keyboard_shortcut_viewer::AcceleratorId> chrome_accelerator_ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutViewerMetadataTest);
};

}  // namespace

// Test that AcceleratorId has at least one corresponding accelerators in ash or
// chrome. Some |accelerator_id| might exist in both accelerator_tables, such as
// new window and new tab.
TEST_F(KeyboardShortcutViewerMetadataTest, CheckAcceleratorIdHasAccelerator) {
  for (const auto& shortcut_item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    if (shortcut_item.description_message_id ==
            IDS_KSV_DESCRIPTION_DESKS_NEW_DESK ||
        shortcut_item.description_message_id ==
            IDS_KSV_DESCRIPTION_DESKS_REMOVE_CURRENT_DESK) {
      // Ignore these for now until https://crbug.com/976487 is fixed.
      // These two accelerators have to be listed differently in the keyboard
      // shortcut viewer than how they are actually defined in the accelerator
      // table due to the SEARCH + "=" and "-" remapping to F11 and F12
      // respectively.
      continue;
    }

    for (const auto& accelerator_id : shortcut_item.accelerator_ids) {
      int number_of_accelerator = ash_accelerator_ids_.count(accelerator_id) +
                                  chrome_accelerator_ids_.count(accelerator_id);
      EXPECT_GE(number_of_accelerator, 1)
          << "accelerator_id has no corresponding accelerator: "
          << AcceleratorIdToString(accelerator_id);
    }
  }
}

// Test that AcceleratorIds have no duplicates.
TEST_F(KeyboardShortcutViewerMetadataTest, CheckAcceleratorIdsNoDuplicates) {
  std::set<keyboard_shortcut_viewer::AcceleratorId> accelerator_ids;
  for (const auto& shortcut_item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    for (const auto& accelerator_id : shortcut_item.accelerator_ids) {
      EXPECT_TRUE(accelerator_ids.insert(accelerator_id).second)
          << "Has duplicated accelerator_id: "
          << AcceleratorIdToString(accelerator_id);
    }
  }
}

// Test that metadata with empty |shortcut_key_codes| and grouped AcceleratorIds
// should have the same modifiers.
TEST_F(KeyboardShortcutViewerMetadataTest,
       CheckModifiersEqualForGroupedAcceleratorIdsWithEmptyShortcutKeyCodes) {
  for (const auto& shortcut_item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    // This test only checks metadata with empty |shortcut_key_codes| and
    // grouped |accelerator_ids|.
    if (!shortcut_item.shortcut_key_codes.empty() ||
        shortcut_item.accelerator_ids.size() <= 1)
      continue;

    const int modifiers = shortcut_item.accelerator_ids[0].modifiers;
    for (const auto& accelerator_id : shortcut_item.accelerator_ids) {
      EXPECT_EQ(modifiers, accelerator_id.modifiers)
          << "Grouped accelerator_ids with empty shortcut_key_codes should "
             "have same modifiers: "
          << AcceleratorIdsToString(shortcut_item.accelerator_ids);
    }
  }
}

// Test that modifying Ash/Chrome accelerator should update
// KeyboardShortcutViewerMetadata. (https://crbug.com/826037).
// 1. If you are adding/deleting/modifying shortcuts, please also
//    add/delete/modify the corresponding strings and items to the list of
//    KeyboardShortcutItem.
// 2. Please update the number and hash value of Ash/Chrome accelerators (these
//    available on Chrome OS) on the top of this file. The new number and hash
//    value will be provided in the test output.
// 3. If there is no corrensponding item in the Keyboard Shortcut Viewer, please
//    consider adding the shortcut to it or only update 2.
TEST_F(KeyboardShortcutViewerMetadataTest,
       ModifyAcceleratorShouldUpdateMetadata) {
  std::vector<ash::AcceleratorData> ash_accelerators;
  std::vector<AcceleratorMapping> chrome_accelerators;
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i)
    ash_accelerators.emplace_back(ash::kAcceleratorData[i]);
  for (const auto& accel_mapping : GetAcceleratorList())
    chrome_accelerators.emplace_back(accel_mapping);

  const char kCommonMessage[] =
      "If you are modifying Chrome OS available shortcuts, please update "
      "Keyboard Shortcut Viewer shortcuts and the following value(s) on the "
      "top of this file:\n";
  const int ash_accelerators_number = ash_accelerators.size();
  EXPECT_EQ(ash_accelerators_number, kAshAcceleratorsTotalNum)
      << kCommonMessage
      << "kAshAcceleratorsTotalNum=" << ash_accelerators_number << "\n";

  std::stable_sort(ash_accelerators.begin(), ash_accelerators.end(),
                   AshAcceleratorDataCmp());
  const std::string ash_accelerators_hash =
      HashAshAcceleratorData(ash_accelerators);
  EXPECT_EQ(ash_accelerators_hash, kAshAcceleratorsHash)
      << kCommonMessage << "kAshAcceleratorsHash=\"" << ash_accelerators_hash
      << "\"\n";

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
