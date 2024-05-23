// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::ContainerEq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StrEq;
using ::testing::UnorderedElementsAreArray;

namespace {

// The PIIMap for testing.
const PIIMap kPIIMap = {
    {redaction::PIIType::kIPAddress, {"0.255.255.255", "::ffff:cb0c:10ea"}},
    {redaction::PIIType::kURL,
     {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x"}},
    {redaction::PIIType::kStableIdentifier,
     {"123e4567-e89b-12d3-a456-426614174000",
      "27540283740a0897ab7c8de0f809add2bacde78f"}}};

const redaction::PIIType kPIITypes[] = {redaction::PIIType::kIPAddress,
                                        redaction::PIIType::kURL,
                                        redaction::PIIType::kStableIdentifier};

}  // namespace

class SupportToolUiUtilsTest : public ::testing::Test {
 public:
  SupportToolUiUtilsTest() = default;

  SupportToolUiUtilsTest(const SupportToolUiUtilsTest&) = delete;
  SupportToolUiUtilsTest& operator=(const SupportToolUiUtilsTest&) = delete;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    browser_manager_ = std::make_unique<crosapi::FakeBrowserManager>();
  }

  void TearDown() override {
    browser_manager_.reset();
    profile_manager_.reset();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Change included field of `included_data_collectors` in `data_collectors` as
  // true for testing.
  void MarkDataCollectorsAsIncluded(
      base::Value::List& data_collectors,
      const std::set<support_tool::DataCollectorType>&
          included_data_collectors) {
    for (auto& data_collector : data_collectors) {
      base::Value::Dict& data_collector_item = data_collector.GetDict();
      std::optional<int> data_collector_enum =
          data_collector_item.FindInt(support_tool_ui::kDataCollectorProtoEnum);
      ASSERT_TRUE(data_collector_enum);
      if (base::Contains(included_data_collectors,
                         static_cast<support_tool::DataCollectorType>(
                             data_collector_enum.value())))
        data_collector_item.Set(support_tool_ui::kDataCollectorIncluded, true);
    }
  }

  std::string GetExpectedPIIDefinitionString(
      const redaction::PIIType& pii_type) {
    // PII types with the definition strings.
    static constexpr auto kExpectedPiiTypeDefinitions =
        base::MakeFixedFlatMap<redaction::PIIType, int>(
            {{redaction::PIIType::kIPAddress, IDS_SUPPORT_TOOL_IP_ADDRESS},
             {redaction::PIIType::kURL, IDS_SUPPORT_TOOL_URLS},
             {redaction::PIIType::kStableIdentifier,
              IDS_SUPPORT_TOOL_STABLE_IDENTIDIERS}});
    auto it = kExpectedPiiTypeDefinitions.find(pii_type);
    EXPECT_NE(kExpectedPiiTypeDefinitions.end(), it);
    std::string definition = l10n_util::GetStringUTF8(it->second);
    return definition;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<crosapi::FakeBrowserManager> browser_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
    const redaction::PIIType pii_type = static_cast<redaction::PIIType>(
        pii_item->FindInt(support_tool_ui::kPiiItemPIITypeKey).value());
    const std::string* description =
        pii_item->FindString(support_tool_ui::kPiiItemDescriptionKey);
    // The dictionary must contain description.
    EXPECT_TRUE(description);
    // The definition string must equal to the expected one in
    // `kPIIStringsWithDefinition`.
    EXPECT_EQ(*description, GetExpectedPIIDefinitionString(pii_type));
    const base::Value::List* pii_data =
        pii_item->FindList(support_tool_ui::kPiiItemDetectedDataKey);
    // Check the detected data.
    EXPECT_TRUE(pii_data);
    EXPECT_THAT(kPIIMap.at(pii_type),
                UnorderedElementsAreArray(
                    base::ToVector(*pii_data, [](const base::Value& value) {
                      // Contents of PII data must be string.
                      return value.GetString();
                    })));
  }

  // Update all PII items to have their keep value as true.
  for (auto& detected_pii_item : detected_pii_items) {
    base::Value::Dict& pii_item = detected_pii_item.GetDict();
    // Remove the `keep` key to update it.
    pii_item.Remove(support_tool_ui::kPiiItemKeepKey);
    pii_item.Set(support_tool_ui::kPiiItemKeepKey, true);
  }

  const std::set<redaction::PIIType>& pii_to_keep_result =
      GetPIITypesToKeep(&detected_pii_items);
  // Check if the returned PII type set is as expected.
  EXPECT_THAT(pii_to_keep_result, UnorderedElementsAreArray(kPIITypes));
}

TEST_F(SupportToolUiUtilsTest, CustomizedUrl) {
  const std::string test_case_id = "test_case_id_0";
  // Get list of all data collectors.
  base::Value::List expected_data_collectors =
      GetAllDataCollectorItemsForDeviceForTesting();
  std::set<support_tool::DataCollectorType> included_data_collectors = {
      support_tool::DataCollectorType::CHROME_INTERNAL,
      support_tool::DataCollectorType::CRASH_IDS};
  MarkDataCollectorsAsIncluded(expected_data_collectors,
                               included_data_collectors);
  base::Value::Dict url_generation_result =
      GenerateCustomizedURL(test_case_id, &expected_data_collectors);
  // The result must be successful.
  EXPECT_TRUE(
      url_generation_result
          .FindBool(support_tool_ui::kSupportTokenGenerationResultSuccess)
          .value());
  // Error string must be empty.
  EXPECT_EQ(*url_generation_result.FindString(
                support_tool_ui::kSupportTokenGenerationResultErrorMessage),
            std::string());
  const std::string* url_output = url_generation_result.FindString(
      support_tool_ui::kSupportTokenGenerationResultToken);
  ASSERT_TRUE(url_output);
  // URL output shouldn't be empty.
  EXPECT_THAT(*url_output, Not(IsEmpty()));
  // Check that case ID in the URL is as expected.
  EXPECT_EQ(GetSupportCaseIDFromURL(GURL(*url_output)), test_case_id);
  // Get the data collector module from URL.
  std::string data_collector_module;
  net::GetValueForKeyInQuery(GURL(*url_output), support_tool_ui::kModuleQuery,
                             &data_collector_module);
  EXPECT_THAT(data_collector_module, Not(IsEmpty()));
  base::Value::List data_collector_items_result =
      GetDataCollectorItemsInQuery(data_collector_module);
  // Check that the output data collector list is equal to expected.
  EXPECT_EQ(data_collector_items_result.size(),
            expected_data_collectors.size());
  for (size_t i = 0; i < data_collector_items_result.size(); i++) {
    const base::Value::Dict& actual_data_collector_item =
        data_collector_items_result[i].GetDict();
    const base::Value::Dict& extected_data_collector_item =
        expected_data_collectors[i].GetDict();
    EXPECT_EQ(actual_data_collector_item
                  .FindInt(support_tool_ui::kDataCollectorProtoEnum)
                  .value(),
              extected_data_collector_item
                  .FindInt(support_tool_ui::kDataCollectorProtoEnum)
                  .value());
    EXPECT_EQ(actual_data_collector_item
                  .FindBool(support_tool_ui::kDataCollectorIncluded)
                  .value(),
              extected_data_collector_item
                  .FindBool(support_tool_ui::kDataCollectorIncluded)
                  .value());
  }
  // Check if the output of GetIncludedDataCollectorTypes is equal to expected
  // set of included data collectors.
  EXPECT_THAT(GetIncludedDataCollectorTypes(&data_collector_items_result),
              ContainerEq(included_data_collectors));
}

TEST_F(SupportToolUiUtilsTest, SupportToken) {
  // Get list of all data collectors.
  base::Value::List expected_data_collectors =
      GetAllDataCollectorItemsForDeviceForTesting();
  std::set<support_tool::DataCollectorType> included_data_collectors = {
      support_tool::DataCollectorType::CHROME_INTERNAL,
      support_tool::DataCollectorType::CRASH_IDS};
  MarkDataCollectorsAsIncluded(expected_data_collectors,
                               included_data_collectors);
  base::Value::Dict support_token_generation_result =
      GenerateSupportToken(&expected_data_collectors);
  // The result must be successful.
  EXPECT_TRUE(
      support_token_generation_result
          .FindBool(support_tool_ui::kSupportTokenGenerationResultSuccess)
          .value());
  // Error string must be empty.
  EXPECT_EQ(*support_token_generation_result.FindString(
                support_tool_ui::kSupportTokenGenerationResultErrorMessage),
            std::string());
  const std::string* token_output = support_token_generation_result.FindString(
      support_tool_ui::kSupportTokenGenerationResultToken);
  ASSERT_TRUE(token_output);
  // Output shouldn't be empty.
  EXPECT_THAT(*token_output, Not(IsEmpty()));
  base::Value::List data_collector_items_result =
      GetDataCollectorItemsInQuery(*token_output);
  // Check that the output data collector list is equal to expected.
  EXPECT_EQ(data_collector_items_result.size(),
            expected_data_collectors.size());
  for (size_t i = 0; i < data_collector_items_result.size(); i++) {
    const base::Value::Dict& actual_data_collector_item =
        data_collector_items_result[i].GetDict();
    const base::Value::Dict& extected_data_collector_item =
        expected_data_collectors[i].GetDict();
    EXPECT_EQ(actual_data_collector_item
                  .FindInt(support_tool_ui::kDataCollectorProtoEnum)
                  .value(),
              extected_data_collector_item
                  .FindInt(support_tool_ui::kDataCollectorProtoEnum)
                  .value());
    EXPECT_EQ(actual_data_collector_item
                  .FindBool(support_tool_ui::kDataCollectorIncluded)
                  .value(),
              extected_data_collector_item
                  .FindBool(support_tool_ui::kDataCollectorIncluded)
                  .value());
  }
  // Check if the output of GetIncludedDataCollectorTypes is equal to expected
  // set of included data collectors.
  EXPECT_THAT(GetIncludedDataCollectorTypes(&data_collector_items_result),
              ContainerEq(included_data_collectors));
}
