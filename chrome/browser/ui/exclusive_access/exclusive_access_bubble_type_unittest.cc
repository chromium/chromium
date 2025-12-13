// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/grit/generated_resources.h"
#include "components/fullscreen_control/fullscreen_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace exclusive_access_bubble {

namespace {

// Keywords to identify strings for different types of ExclusiveAccessBubble.
constexpr char16_t kExitFullscreenString[] = u"exit full screen";
constexpr char16_t kPointerLockString[] = u"show your cursor";
constexpr char16_t kDownloadString[] = u"Download started";

class ExclusiveAccessBubbleTypeTest : public testing::Test {
 public:
  ExclusiveAccessBubbleTypeTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFullscreenBubbleShowOrigin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test GetBubbleText with an empty URL.
TEST_F(ExclusiveAccessBubbleTypeTest, GetBubbleTextEmptyURL) {
  std::u16string accelerator = u"Esc";
  bool has_download = false;
  bool notify_overridden = false;
  std::u16string actual_text = GetInstructionTextForType(
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION, accelerator,
      std::nullopt, has_download, notify_overridden);

  EXPECT_TRUE(base::Contains(actual_text, accelerator));
  EXPECT_TRUE(base::Contains(actual_text, kExitFullscreenString));
}

// Test GetBubbleText with a non-empty URL.
TEST_F(ExclusiveAccessBubbleTypeTest, GetBubbleTextNonEmptyURL) {
  std::u16string accelerator = u"Esc";
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  bool has_download = false;
  bool notify_overridden = false;
  std::u16string actual_text = GetInstructionTextForType(
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION, accelerator,
      std::make_optional<std::u16string>(base::UTF8ToUTF16(origin.Serialize())),
      has_download, notify_overridden);

  EXPECT_TRUE(base::Contains(actual_text, accelerator));
  EXPECT_TRUE(base::Contains(actual_text, u"example.com"));
  EXPECT_TRUE(base::Contains(actual_text, kExitFullscreenString));
}

// Test GetBubbleText with a download and no override.
TEST_F(ExclusiveAccessBubbleTypeTest, GetBubbleTextWithDownloadNoOverride) {
  std::u16string accelerator = u"Esc";
  bool has_download = true;
  bool notify_overridden = false;
  std::u16string actual_text = GetInstructionTextForType(
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION, accelerator,
      std::nullopt, has_download, notify_overridden);

  EXPECT_TRUE(base::Contains(actual_text, accelerator));
  EXPECT_TRUE(base::Contains(actual_text, kDownloadString));
}

// Test GetInstructionTextForType for pointer lock exit instruction.
TEST_F(ExclusiveAccessBubbleTypeTest,
       GetInstructionTextForTypePointerLockExit) {
  std::u16string accelerator = u"Esc";
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  bool has_download = false;
  bool notify_overridden = false;
  std::u16string actual_text = GetInstructionTextForType(
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION, accelerator,
      std::make_optional<std::u16string>(base::UTF8ToUTF16(origin.Serialize())),
      has_download, notify_overridden);

  EXPECT_TRUE(base::Contains(actual_text, accelerator));
  EXPECT_TRUE(base::Contains(actual_text, u"example.com"));
  EXPECT_TRUE(base::Contains(actual_text, kPointerLockString));
}

// Test GetInstructionTextForType for keyboard lock exit instruction.
TEST_F(ExclusiveAccessBubbleTypeTest,
       GetInstructionTextForTypeKeyboardLockExit) {
  std::u16string accelerator = u"Esc";
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  bool has_download = false;
  bool notify_overridden = false;
  std::u16string actual_text = GetInstructionTextForType(
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION, accelerator,
      std::make_optional<std::u16string>(base::UTF8ToUTF16(origin.Serialize())),
      has_download, notify_overridden);

  EXPECT_TRUE(base::Contains(actual_text, accelerator));
  EXPECT_TRUE(base::Contains(actual_text, u"example.com"));
  EXPECT_TRUE(base::Contains(actual_text, kExitFullscreenString));
}

}  // namespace

}  // namespace exclusive_access_bubble
