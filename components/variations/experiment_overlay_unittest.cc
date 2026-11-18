// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/experiment_overlay.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "components/variations/variations_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

BASE_FEATURE(kExperimentOverlayUnexposedTestFeatureA,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kExperimentOverlayUnexposedTestFeatureB,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kTestStudyName[] = "AIFuseboxIOSModelPicker";
constexpr char kTestGroupName[] = "Enabled_Arm2";
constexpr char kTestSource[] = "finch_gcl";
constexpr char kTestFeatureName[] = "OverlayTestFeature";
constexpr char kParamModelTier[] = "model_tier";
constexpr char kParamValuePro[] = "pro";
constexpr char kParamMaxItems[] = "max_items";
constexpr char kParamValueFour[] = "4";
constexpr char kTestVariationId[] = "3391024";
constexpr char kTestTriggerVariationId[] = "t3391025";
constexpr char kConflictFeatureName[] = "ConflictFeature";
constexpr uint32_t kTestLayerId = 42;
constexpr uint32_t kPinnedMemberId = 1;

// Returns the smallest valid overlay payload dictionary.
base::DictValue CreateMinimalPayloadDict() {
  base::DictValue dict;
  dict.Set(ExperimentOverlayConfig::kKeyVersion,
           ExperimentOverlayConfig::kCurrentPayloadVersion);
  dict.Set(ExperimentOverlayConfig::kKeyStudyName, kTestStudyName);
  dict.Set(ExperimentOverlayConfig::kKeyGroupName, kTestGroupName);
  return dict;
}

base::ListValue CreateStringList(const std::vector<std::string>& values) {
  base::ListValue list;
  for (const std::string& value : values) {
    list.Append(value);
  }
  return list;
}

class ExperimentOverlayTest : public ::testing::Test {
 protected:
  ExperimentOverlayTest() {
    scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a full overlay configuration round-trips through a dictionary.
TEST_F(ExperimentOverlayTest, ToDictAndFromDictRoundTrip) {
  ExperimentOverlayConfig config;
  config.source = kTestSource;
  config.study_name = kTestStudyName;
  config.group_name = kTestGroupName;
  config.enable_features = {kExperimentOverlayUnexposedTestFeatureA.name};
  config.disable_features = {kExperimentOverlayUnexposedTestFeatureB.name};
  config.params[kParamModelTier] = kParamValuePro;
  config.params[kParamMaxItems] = kParamValueFour;
  config.variation_ids = {kTestVariationId, kTestTriggerVariationId};
  config.layer = ExperimentOverlayConfig::LayerMemberSpec{
      .layer_id = kTestLayerId,
      .layer_member_id = kPinnedMemberId,
  };

  base::DictValue dict = config.ToDict();
  std::optional<ExperimentOverlayConfig> decoded =
      ExperimentOverlayConfig::FromDict(dict);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, config);
}

// Full wire payload pinned independently of the `kKey*` constants so key
// renames or swaps break this test before they break external producers.
constexpr char kFullWirePayload[] = R"({
  "v": 1, "src": "finch_gcl", "s": "AIFuseboxIOSModelPicker",
  "g": "Enabled_Arm2", "ef": ["FeatureA"], "df": ["FeatureB"],
  "p": {"max_items": "4"}, "vid": ["3391024", "t3391025"],
  "l": {"id": 42, "m": 1}
})";

// Tests that `FromDict()` and `ToDict()` match a literal full wire payload.
TEST_F(ExperimentOverlayTest, FromDictAndToDictMatchFullWirePayload) {
  std::optional<ExperimentOverlayConfig> config =
      ExperimentOverlayConfig::FromDict(
          base::test::ParseJsonDict(kFullWirePayload));
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->source, "finch_gcl");
  EXPECT_EQ(config->study_name, "AIFuseboxIOSModelPicker");
  EXPECT_EQ(config->group_name, "Enabled_Arm2");
  EXPECT_THAT(config->enable_features, testing::ElementsAre("FeatureA"));
  EXPECT_THAT(config->disable_features, testing::ElementsAre("FeatureB"));
  EXPECT_THAT(config->params,
              testing::ElementsAre(testing::Pair("max_items", "4")));
  EXPECT_THAT(config->variation_ids,
              testing::ElementsAre("3391024", "t3391025"));
  ASSERT_TRUE(config->layer.has_value());
  EXPECT_EQ(config->layer->layer_id, 42u);
  EXPECT_EQ(config->layer->layer_member_id, 1u);
  EXPECT_THAT(config->ToDict(), base::test::IsJson(kFullWirePayload));
}

// Tests that a minimal literal payload defaults `src` and that `ToDict()`
// omits empty optional fields.
TEST_F(ExperimentOverlayTest, FromDictAndToDictMatchMinimalWirePayload) {
  std::optional<ExperimentOverlayConfig> config =
      ExperimentOverlayConfig::FromDict(
          base::test::ParseJsonDict(R"({"v": 1, "s": "Study", "g": "Group"})"));
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->source, ExperimentOverlayConfig::kDefaultSource);
  EXPECT_THAT(config->ToDict(),
              base::test::IsJson(
                  R"({"v": 1, "src": "json", "s": "Study", "g": "Group"})"));
}

// Tests that `GetAllReferencedFeatures()` returns the sorted union of the
// enabled and disabled feature lists.
TEST_F(ExperimentOverlayTest, GetAllReferencedFeaturesReturnsUnion) {
  ExperimentOverlayConfig config;
  EXPECT_TRUE(config.GetAllReferencedFeatures().empty());

  config.enable_features = {"FeatureB", "FeatureA"};
  config.disable_features = {"FeatureC"};
  EXPECT_THAT(config.GetAllReferencedFeatures(),
              testing::ElementsAre("FeatureA", "FeatureB", "FeatureC"));
}

// Tests that a payload without a version is rejected.
TEST_F(ExperimentOverlayTest, FromDictRejectsMissingVersion) {
  base::DictValue dict = CreateMinimalPayloadDict();
  dict.Remove(ExperimentOverlayConfig::kKeyVersion);
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

// Tests that a payload with an unsupported version is rejected.
TEST_F(ExperimentOverlayTest, FromDictRejectsUnsupportedVersion) {
  base::DictValue dict = CreateMinimalPayloadDict();
  dict.Set(ExperimentOverlayConfig::kKeyVersion,
           ExperimentOverlayConfig::kCurrentPayloadVersion + 1);
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

class ExperimentOverlayWrongTypeTest
    : public testing::TestWithParam<const char*> {};

// Tests that an optional field with a non-matching JSON type is rejected.
TEST_P(ExperimentOverlayWrongTypeTest, FromDictRejectsWrongType) {
  base::DictValue dict = CreateMinimalPayloadDict();
  dict.Set(GetParam(), 123);
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentOverlayWrongTypeTest,
                         testing::Values("src", "ef", "df", "p", "vid", "l"));

// Describes whether `name` is accepted in the payload field `key`.
struct NameTestCase {
  const char* test_name;
  const char* key;
  const char* name;
  bool accepted;
};

class ExperimentOverlayNameTest : public testing::TestWithParam<NameTestCase> {
};

// Tests study and group name validation in `FromDict()`.
TEST_P(ExperimentOverlayNameTest, FromDictValidatesName) {
  base::DictValue dict = CreateMinimalPayloadDict();
  dict.Set(GetParam().key, GetParam().name);
  EXPECT_EQ(ExperimentOverlayConfig::FromDict(dict).has_value(),
            GetParam().accepted);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExperimentOverlayNameTest,
    testing::Values(NameTestCase{"StudyEmpty", "s", "", false},
                    NameTestCase{"StudySlash", "s", "Study/Name", false},
                    NameTestCase{"StudyComma", "s", "Study,Name", false},
                    NameTestCase{"StudyLessThan", "s", "Study<Name", false},
                    NameTestCase{"StudyAsterisk", "s", "Study*Name", false},
                    NameTestCase{"StudyControl", "s", "Study\x01Name", false},
                    NameTestCase{"StudyTab", "s", "Study\tName", false},
                    NameTestCase{"StudyDel", "s", "Study\x7FName", false},
                    NameTestCase{"StudyUtf8", "s", "Study\xC3\xA9Name", false},
                    NameTestCase{"StudyPeriodAndColon", "s", "Study.Name:V2",
                                 true},
                    NameTestCase{"GroupEmpty", "g", "", false},
                    NameTestCase{"GroupSlash", "g", "Group/Name", false},
                    NameTestCase{"GroupLessThan", "g", "Group<Name", false},
                    NameTestCase{"GroupAsterisk", "g", "Group*Name", false},
                    NameTestCase{"GroupControl", "g", "Group\x01Name", false},
                    NameTestCase{"GroupTab", "g", "Group\tName", false},
                    NameTestCase{"GroupDel", "g", "Group\x7FName", false},
                    NameTestCase{"GroupUtf8", "g", "Group\xC3\xA9Name", false},
                    NameTestCase{"GroupListSeparators", "g",
                                 "Enabled,max_queue_time:75ms,_V4.1", true}),
    [](const testing::TestParamInfo<NameTestCase>& info) {
      return info.param.test_name;
    });

// Describes whether `name` is accepted in either feature list.
struct FeatureNameTestCase {
  const char* test_name;
  const char* name;
  bool accepted;
};

class ExperimentOverlayFeatureNameTest
    : public testing::TestWithParam<FeatureNameTestCase> {};

// Tests feature name validation in both feature lists in `FromDict()`.
TEST_P(ExperimentOverlayFeatureNameTest, FromDictValidatesFeatureName) {
  for (const char* list_key : {"ef", "df"}) {
    SCOPED_TRACE(list_key);
    base::DictValue dict = CreateMinimalPayloadDict();
    dict.Set(list_key, CreateStringList({GetParam().name}));
    EXPECT_EQ(ExperimentOverlayConfig::FromDict(dict).has_value(),
              GetParam().accepted);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExperimentOverlayFeatureNameTest,
    testing::Values(
        FeatureNameTestCase{"Empty", "", false},
        FeatureNameTestCase{"Slash", "Feature/X", false},
        FeatureNameTestCase{"Comma", "Feature,X", false},
        FeatureNameTestCase{"LessThan", "Feature<X", false},
        FeatureNameTestCase{"Asterisk", "Feature*X", false},
        FeatureNameTestCase{"Control", "Feature\x01X", false},
        FeatureNameTestCase{"Tab", "Feature\tX", false},
        FeatureNameTestCase{"Del", "Feature\x7FX", false},
        FeatureNameTestCase{"Utf8", "Feature\xC3\xA9X", false},
        FeatureNameTestCase{"OptInFlag", "EnableExperimentOverlay", false},
        FeatureNameTestCase{"KillSwitch", "ExperimentOverlayKillSwitch", false},
        FeatureNameTestCase{"PeriodAndColon", "Feature.Name:V2", true}),
    [](const testing::TestParamInfo<FeatureNameTestCase>& info) {
      return info.param.test_name;
    });

// Tests that duplicate entries within a feature list are rejected.
TEST_F(ExperimentOverlayTest, FromDictRejectsDuplicateFeature) {
  for (const char* list_key : {ExperimentOverlayConfig::kKeyEnableFeatures,
                               ExperimentOverlayConfig::kKeyDisableFeatures}) {
    SCOPED_TRACE(list_key);
    base::DictValue dict = CreateMinimalPayloadDict();
    dict.Set(list_key, CreateStringList({kTestFeatureName, kConflictFeatureName,
                                         kTestFeatureName}));
    EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
  }
}

// Tests that a feature present in both feature lists is rejected.
TEST_F(ExperimentOverlayTest, FromDictRejectsFeatureInBothLists) {
  base::DictValue dict = CreateMinimalPayloadDict();
  dict.Set(ExperimentOverlayConfig::kKeyEnableFeatures,
           CreateStringList({kConflictFeatureName}));
  dict.Set(ExperimentOverlayConfig::kKeyDisableFeatures,
           CreateStringList({kConflictFeatureName}));
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

// Tests that an empty parameter key is rejected.
TEST_F(ExperimentOverlayTest, FromDictRejectsEmptyParamKey) {
  base::DictValue dict = CreateMinimalPayloadDict();
  base::DictValue params;
  params.Set("", kParamValuePro);
  dict.Set(ExperimentOverlayConfig::kKeyParams, std::move(params));
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

// Describes a list of variation IDs that `FromDict()` must reject.
struct InvalidVariationIdsTestCase {
  const char* test_name;
  std::vector<std::string> variation_ids;
};

class ExperimentOverlayInvalidVariationIdsTest
    : public testing::TestWithParam<InvalidVariationIdsTestCase> {};

// Tests that non-numeric, non-positive, or duplicate variation IDs fail.
TEST_P(ExperimentOverlayInvalidVariationIdsTest, FromDictRejects) {
  base::DictValue dict = CreateMinimalPayloadDict();
  dict.Set(ExperimentOverlayConfig::kKeyVariationIds,
           CreateStringList(GetParam().variation_ids));
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExperimentOverlayInvalidVariationIdsTest,
    testing::Values(
        InvalidVariationIdsTestCase{"NonNumeric", {"abc"}},
        InvalidVariationIdsTestCase{"Zero", {"0"}},
        InvalidVariationIdsTestCase{"Negative", {"-42"}},
        InvalidVariationIdsTestCase{"TriggerOnly", {"t"}},
        InvalidVariationIdsTestCase{"TriggerZero", {"t0"}},
        InvalidVariationIdsTestCase{"PlusSign", {"+3391024"}},
        InvalidVariationIdsTestCase{"TriggerPlusSign", {"t+3391024"}},
        InvalidVariationIdsTestCase{"Duplicate", {"3391024", "3391024"}},
        InvalidVariationIdsTestCase{"DuplicateAcrossTrigger",
                                    {"3391024", "t3391024"}}),
    [](const testing::TestParamInfo<InvalidVariationIdsTestCase>& info) {
      return info.param.test_name;
    });

class ExperimentOverlayInvalidLayerTest
    : public testing::TestWithParam<std::pair<int, int>> {};

// Tests that a layer without positive layer and member IDs is rejected.
TEST_P(ExperimentOverlayInvalidLayerTest, FromDictRejects) {
  base::DictValue dict = CreateMinimalPayloadDict();
  base::DictValue layer;
  layer.Set(ExperimentOverlayConfig::kKeyLayerId, GetParam().first);
  layer.Set(ExperimentOverlayConfig::kKeyLayerMemberId, GetParam().second);
  dict.Set(ExperimentOverlayConfig::kKeyLayer, std::move(layer));
  EXPECT_FALSE(ExperimentOverlayConfig::FromDict(dict).has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentOverlayInvalidLayerTest,
                         testing::Values(std::pair(0, 1),
                                         std::pair(1, 0),
                                         std::pair(-1, 1),
                                         std::pair(1, -1)));

// Tests that variation ID parsing accepts ASCII digits after an optional 't'.
TEST_F(ExperimentOverlayTest, ParseVariationIdAcceptsDigits) {
  EXPECT_EQ(ParseExperimentOverlayVariationId("3391024"), 3391024);
  EXPECT_EQ(ParseExperimentOverlayVariationId("t3391024"), 3391024);
}

class ExperimentOverlayInvalidVariationIdParseTest
    : public testing::TestWithParam<const char*> {};

// Tests that variation ID parsing rejects anything but positive ASCII digits.
TEST_P(ExperimentOverlayInvalidVariationIdParseTest, ReturnsNullopt) {
  EXPECT_FALSE(ParseExperimentOverlayVariationId(GetParam()).has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentOverlayInvalidVariationIdParseTest,
                         testing::Values("",
                                         "t",
                                         "0",
                                         "t0",
                                         "-5",
                                         "+5",
                                         "t+5",
                                         " 5",
                                         "5 ",
                                         "5a",
                                         "tt5",
                                         "99999999999"));

// Tests that channel gating allows only Canary, Dev, and unbranded builds.
TEST_F(ExperimentOverlayTest, ChannelGating) {
  EXPECT_TRUE(IsExperimentOverlayAllowed(version_info::Channel::UNKNOWN));
  EXPECT_TRUE(IsExperimentOverlayAllowed(version_info::Channel::CANARY));
  EXPECT_TRUE(IsExperimentOverlayAllowed(version_info::Channel::DEV));
  EXPECT_FALSE(IsExperimentOverlayAllowed(version_info::Channel::BETA));
  EXPECT_FALSE(IsExperimentOverlayAllowed(version_info::Channel::STABLE));
}

// Tests that feature gating requires user opt-in and no active kill switch.
TEST_F(ExperimentOverlayTest, FeatureGating) {
  EXPECT_FALSE(IsExperimentOverlayFeatureEnabled());

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        /*enabled_features=*/{kEnableExperimentOverlay},
        /*disabled_features=*/{});
    EXPECT_TRUE(IsExperimentOverlayFeatureEnabled());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        /*enabled_features=*/{kEnableExperimentOverlay,
                              kExperimentOverlayKillSwitch},
        /*disabled_features=*/{});
    EXPECT_FALSE(IsExperimentOverlayFeatureEnabled());
  }
}

}  // namespace
}  // namespace variations
