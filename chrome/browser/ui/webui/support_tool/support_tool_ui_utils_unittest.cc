// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"

#include <set>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/pii_types.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;
using ::testing::UnorderedElementsAreArray;

namespace {

// The PIIMap for testing.
const PIIMap kPIIMap = {
    {feedback::PIIType::kIPAddress, {"0.255.255.255", "::ffff:cb0c:10ea"}},
    {feedback::PIIType::kURL,
     {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x"}},
    {feedback::PIIType::kStableIdentifier,
     {"123e4567-e89b-12d3-a456-426614174000",
      "27540283740a0897ab7c8de0f809add2bacde78f"}}};

const feedback::PIIType kPIITypes[] = {feedback::PIIType::kIPAddress,
                                       feedback::PIIType::kURL,
                                       feedback::PIIType::kStableIdentifier};

// PII strings with the definition strings.
const auto kPIIStringsWithDefinition = base::MakeFixedFlatMap<
    base::StringPiece,
    base::StringPiece>(
    {{support_tool_ui::kIPAddress, "0.255.255.255, ::ffff:cb0c:10ea"},
     {support_tool_ui::kURL,
      "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x"},
     {support_tool_ui::kStableIdentifier,
      "123e4567-e89b-12d3-a456-426614174000, "
      "27540283740a0897ab7c8de0f809add2bacde78f"}});

}  // namespace

class SupportToolUiUtilsTest : public ::testing::Test {
 public:
  SupportToolUiUtilsTest() = default;

  SupportToolUiUtilsTest(const SupportToolUiUtilsTest&) = delete;
  SupportToolUiUtilsTest& operator=(const SupportToolUiUtilsTest&) = delete;
};

TEST_F(SupportToolUiUtilsTest, PiiItems) {
  // Get the list of detected PII items in PIIMap.
  base::Value::List detected_pii_items = GetDetectedPIIDataItems(kPIIMap);
  EXPECT_EQ(detected_pii_items.size(), kPIIMap.size());
  // Check the contents of `detected_pii_items` list.
  for (const auto& detected_pii_item : detected_pii_items) {
    const base::Value::Dict* pii_item = detected_pii_item.GetIfDict();
    // PIIItem must be a Value::Dict.
    EXPECT_TRUE(pii_item);
    const std::string* description =
        pii_item->FindString(support_tool_ui::kPiiItemDescriptionKey);
    // The dictionary must contain description.
    EXPECT_TRUE(description);
    const auto* pii_string_with_definition =
        kPIIStringsWithDefinition.find(*description);
    // The definition string must exist in `kPIIStringsWithDefinition`.
    EXPECT_NE(pii_string_with_definition, kPIIStringsWithDefinition.end());
    const std::string* pii_data =
        pii_item->FindString(support_tool_ui::kPiiItemDetectedDataKey);
    // Check the detected data.
    EXPECT_THAT(*pii_data, StrEq(pii_string_with_definition->second));
  }

  // Update all PII items to have their keep value as true.
  for (auto& detected_pii_item : detected_pii_items) {
    base::Value::Dict& pii_item = detected_pii_item.GetDict();
    // Remove the `keep` key to update it.
    pii_item.Remove(support_tool_ui::kPiiItemKeepKey);
    pii_item.Set(support_tool_ui::kPiiItemKeepKey, true);
  }

  const std::set<feedback::PIIType>& pii_to_keep_result =
      GetPIITypesToKeep(&detected_pii_items);
  // Check if the returned PII type set is as expected.
  EXPECT_THAT(pii_to_keep_result, UnorderedElementsAreArray(kPIITypes));
}
