// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_metrics.h"

#include <string_view>
#include <tuple>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "components/skills/public/skill_metrics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

class SkillsMetricsTest : public testing::Test {
 protected:
  // Helper function to map the enum to the expected XML string.
  std::string_view GetEntryPointString(SkillsDialogEntryPoint entry_point) {
    switch (entry_point) {
      case SkillsDialogEntryPoint::kWebClientBlank:
        return ".WebClient.Blank";
      case SkillsDialogEntryPoint::kWebClientPrefilled:
        return ".WebClient.Prefilled";
      case SkillsDialogEntryPoint::kWebClientRemix:
        return ".WebClient.Remix";
      case SkillsDialogEntryPoint::kManagementPageBlank:
        return ".ManagementPage.Blank";
      case SkillsDialogEntryPoint::kManagementPagePrefilled:
        return ".ManagementPage.Prefilled";
      case SkillsDialogEntryPoint::kManagementPageRemix:
        return ".ManagementPage.Remix";
      case SkillsDialogEntryPoint::kUnknown:
        return ".Unknown";
    }
    NOTREACHED();
  }

  // Helper function to map the enum to the expected XML string for page
  // variants.
  std::string_view GetSkillsManagementPageString(
      mojom::SkillsManagementPage page) {
    switch (page) {
      case mojom::SkillsManagementPage::kErrorPage:
        return "ErrorPage";
      case mojom::SkillsManagementPage::kYourSkills:
        return "YourSkills";
      case mojom::SkillsManagementPage::kBrowseSkills:
        return "BrowseSkills";
    }
    NOTREACHED();
  }
};

TEST_F(SkillsMetricsTest, CheckEntryPointVariantNames) {
  std::optional<base::HistogramVariantsEntryMap> entry_points;
  std::vector<std::string> missing_variants;

  {
    // Reading XML from disk requires blocking permissions in Chromium tests.
    base::ScopedAllowBlockingForTesting allow_blocking;
    entry_points = base::ReadVariantsFromHistogramsXml(
        "SkillsDialogEntryPointAndState", "skills");
    ASSERT_TRUE(entry_points.has_value());
  }

  // Loop through every enum value and verify its specific slice exists.
  for (int i = 0; i <= static_cast<int>(SkillsDialogEntryPoint::kMaxValue);
       ++i) {
    auto entry_point = static_cast<SkillsDialogEntryPoint>(i);
    std::string expected_variant(GetEntryPointString(entry_point));

    if (!entry_points->contains(expected_variant)) {
      missing_variants.push_back(expected_variant);
    }
  }

  // Assert that we didn't miss any variants.
  ASSERT_TRUE(missing_variants.empty())
      << "SkillsDialogEntryPointAndState variants:\n"
      << base::JoinString(missing_variants, ", ")
      << "\nconfigured in skills_metrics.cc but no "
         "corresponding variants were added to the EntryPointAndState token in "
         "//tools/metrics/histograms/metadata/skills/histograms.xml";

  // Verify total count to ensure no extra stale variants exist in the XML.
  EXPECT_EQ(static_cast<size_t>(SkillsDialogEntryPoint::kMaxValue) + 2,
            entry_points->size())
      << "The number of variants in histograms.xml does not match the enum.";
}

struct SkillsMetricsTestParams {
  std::string test_name;

  // Input Data
  bool has_skill = true;
  std::string skill_id = "default_id";
  sync_pb::SkillSource skill_source =
      sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN;
  bool is_web_client = true;

  // Expected Output Data
  bool expected_is_edit_mode;
  SkillsDialogEntryPoint expected_entrypoint;
};

class SkillsMetricsStateResolutionTest
    : public testing::TestWithParam<SkillsMetricsTestParams> {
 protected:
  // Helper to construct the std::optional<Skill> based on test parameters
  std::optional<Skill> GetSkillFromParams(const SkillsMetricsTestParams& p) {
    if (!p.has_skill) {
      return std::nullopt;
    }
    Skill skill;
    skill.id = p.skill_id;
    skill.source = p.skill_source;
    return skill;
  }
};

TEST_P(SkillsMetricsStateResolutionTest, EvaluatesCorrectly) {
  const SkillsMetricsTestParams& p = GetParam();
  std::optional<Skill> skill = GetSkillFromParams(p);

  // Convert the optional to a pointer.
  const Skill* skill_ptr = skill ? &skill.value() : nullptr;

  // Assert IsEditMode matches expectations.
  EXPECT_EQ(IsEditMode(skill_ptr), p.expected_is_edit_mode)
      << "IsEditMode logic failed for test case: " << p.test_name;

  // Route to the correct new function based on the boolean in the test params.
  SkillsDialogEntryPoint actual_entrypoint;
  if (p.is_web_client) {
    actual_entrypoint = ResolveEntryPointForWebClient(skill_ptr);
  } else {
    actual_entrypoint = ResolveEntryPointForManagementPage(skill_ptr);
  }

  // Assert the resolved entry point matches expectations.
  EXPECT_EQ(actual_entrypoint, p.expected_entrypoint)
      << "ResolveEntryPoint logic failed for test case: " << p.test_name;
}

std::string GenerateSkillsMetricsTestName(
    const testing::TestParamInfo<SkillsMetricsTestParams>& info) {
  return info.param.test_name;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SkillsMetricsStateResolutionTest,
    testing::Values(
        // --- BLANK DIALOG CASES (Creation) ---
        SkillsMetricsTestParams{
            .test_name = "NulloptWebClient",
            .has_skill = false,
            .is_web_client = true,
            .expected_is_edit_mode = false,
            .expected_entrypoint = SkillsDialogEntryPoint::kWebClientBlank},
        SkillsMetricsTestParams{
            .test_name = "NulloptMgmtPage",
            .has_skill = false,
            .is_web_client = false,
            .expected_is_edit_mode = false,
            .expected_entrypoint =
                SkillsDialogEntryPoint::kManagementPageBlank},
        SkillsMetricsTestParams{
            .test_name = "EmptyIdWebClient",
            .skill_id = "",  // Empty ID means it's still considered blank/new
            .is_web_client = true,
            .expected_is_edit_mode = false,
            .expected_entrypoint = SkillsDialogEntryPoint::kWebClientBlank},

        // --- FIRST PARTY TEMPLATE CASES (Creation Remix) ---
        SkillsMetricsTestParams{
            .test_name = "FirstPartyWebClient",
            .skill_source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY,
            .is_web_client = true,
            .expected_is_edit_mode = false,
            .expected_entrypoint = SkillsDialogEntryPoint::kWebClientRemix},
        SkillsMetricsTestParams{
            .test_name = "FirstPartyMgmtPage",
            .skill_source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY,
            .is_web_client = false,
            .expected_is_edit_mode = false,
            .expected_entrypoint =
                SkillsDialogEntryPoint::kManagementPageRemix},

        // --- DERIVED USER SKILL CASES (Edit Remix) ---
        SkillsMetricsTestParams{
            .test_name = "DerivedWebClient",
            .skill_source =
                sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY,
            .is_web_client = true,
            .expected_is_edit_mode = true,
            .expected_entrypoint = SkillsDialogEntryPoint::kWebClientRemix},
        SkillsMetricsTestParams{
            .test_name = "DerivedMgmtPage",
            .skill_source =
                sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY,
            .is_web_client = false,
            .expected_is_edit_mode = true,
            .expected_entrypoint =
                SkillsDialogEntryPoint::kManagementPageRemix},

        // --- STANDARD USER SKILL CASES (Edit Prefilled) ---
        SkillsMetricsTestParams{
            .test_name = "UserCreatedWebClient",
            .skill_source = sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED,
            .is_web_client = true,
            .expected_is_edit_mode = true,
            .expected_entrypoint = SkillsDialogEntryPoint::kWebClientPrefilled},
        SkillsMetricsTestParams{
            .test_name = "UserCreatedMgmtPage",
            .skill_source = sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED,
            .is_web_client = false,
            .expected_is_edit_mode = true,
            .expected_entrypoint =
                SkillsDialogEntryPoint::kManagementPagePrefilled}),
    GenerateSkillsMetricsTestName);

TEST_F(SkillsMetricsTest, CheckSkillsManagementPageVariants) {
  std::optional<base::HistogramVariantsEntryMap> page_variants;
  std::vector<std::string> missing_variants;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    page_variants = base::ReadVariantsFromHistogramsXml("Page", "skills");
    ASSERT_TRUE(page_variants.has_value());
  }

  for (int i = 0; i <= static_cast<int>(mojom::SkillsManagementPage::kMaxValue);
       ++i) {
    auto page = static_cast<mojom::SkillsManagementPage>(i);
    std::string expected_variant(GetSkillsManagementPageString(page));

    if (!page_variants->contains(expected_variant)) {
      missing_variants.push_back(expected_variant);
    }
  }

  ASSERT_TRUE(missing_variants.empty())
      << "SkillsManagementPage variants:\n"
      << base::JoinString(missing_variants, ", ")
      << "\nconfigured in skills_metrics.cc but no "
         "corresponding variants were added to the Page token in "
         "//tools/metrics/histograms/metadata/skills/histograms.xml";

  EXPECT_EQ(static_cast<size_t>(mojom::SkillsManagementPage::kMaxValue) + 1,
            page_variants->size())
      << "The number of variants in histograms.xml does not match the enum.";
}

}  // namespace skills
