// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

namespace {

const char kTestPolicy[] = "unit_test.test_policy";
const char kTestPref[] = "unit_test.test_pref";
const char kPolicyName[] = "PolicyForTesting";

constexpr char kValidationSchemaJson[] = R"(
    {
      "type": "object",
      "properties": {
        "PolicyForTesting": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "movie": { "type": "string" },
              "min_age": { "type": "integer" }
            }
          }
        }
      }
    })";

constexpr char kPolicyMapJsonValid[] = R"(
    {
      "PolicyForTesting": [
        "{ \"movie\": \"Talking Animals\", \"min_age\": 0, }",
        "{ \"movie\": \"Five Cowboys\", \"min_age\": 12, }",
        "{ \"movie\": \"Scary Horrors\", \"min_age\": 16, }",
      ]
    })";

constexpr char kPolicyMapJsonInvalid[] = R"(
    {
      "PolicyForTesting": [
        "{ \"movie\": \"Talking Animals\", \"min_age\": \"G\", }",
        "{ \"movie\": \"Five Cowboys\", \"min_age\": \"PG\", }",
        "{ \"movie\": \"Scary Horrors\", \"min_age\": \"R\", }",
      ]
    })";

constexpr char kPolicyMapJsonUnparsable[] = R"(
    {
      "PolicyForTesting": [
        "Talking Animals is rated G",
        "Five Cowboys is rated PG",
        "Scary Horrors is rated R",
      ]
    })";

constexpr char kPolicyMapJsonWrongTypes[] = R"(
    {
      "PolicyForTesting": [ 1, 2, 3, ]
    })";

constexpr char kPolicyMapJsonWrongRootType[] = R"(
    {
      "PolicyForTesting": "test",
    })";

void GetIntegerTypeMap(
    std::vector<std::unique_ptr<StringMappingListPolicyHandler::MappingEntry>>*
        result) {
  result->push_back(
      std::make_unique<StringMappingListPolicyHandler::MappingEntry>(
          "one", std::make_unique<base::Value>(1)));
  result->push_back(
      std::make_unique<StringMappingListPolicyHandler::MappingEntry>(
          "two", std::make_unique<base::Value>(2)));
}

class TestSchemaValidatingPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  TestSchemaValidatingPolicyHandler(const Schema& schema,
                                    SchemaOnErrorStrategy strategy)
      : SchemaValidatingPolicyHandler(kPolicyName, schema, strategy) {}
  ~TestSchemaValidatingPolicyHandler() override {}

  void ApplyPolicySettings(const policy::PolicyMap&, PrefValueMap*) override {}

  bool CheckAndGetValueForTest(const PolicyMap& policies,
                               PolicyErrorMap* errors,
                               std::unique_ptr<base::Value>* value) {
    return SchemaValidatingPolicyHandler::CheckAndGetValue(policies, errors,
                                                           value);
  }
};

// Simple implementation of ListPolicyHandler that assumes a string list and
// sets the kTestPref pref to the filtered list.
class StringListPolicyHandler : public ListPolicyHandler {
 public:
  StringListPolicyHandler(const char* policy_name, const char* pref_path)
      : ListPolicyHandler(policy_name, base::Value::Type::STRING) {}

 protected:
  void ApplyList(base::Value filtered_list, PrefValueMap* prefs) override {
    DCHECK(filtered_list.is_list());
    prefs->SetValue(kTestPref, std::move(filtered_list));
  }
};

std::unique_ptr<SimpleJsonStringSchemaValidatingPolicyHandler>
JsonStringHandlerForTesting() {
  std::string error;
  Schema validation_schema = Schema::Parse(kValidationSchemaJson, &error);
  return std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
      kPolicyName, kTestPref, validation_schema,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED);
}

}  // namespace

TEST(ListPolicyHandlerTest, CheckPolicySettings) {
  base::Value::List list;
  base::Value dict(base::Value::Type::DICT);
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  StringListPolicyHandler handler(kTestPolicy, kTestPref);

  // No policy set is OK.
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  errors.Clear();

  // Not a list is not OK.
  policy_map.Set(kTestPolicy, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 dict.Clone(), nullptr);
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  errors.Clear();

  // Empty list is OK.
  policy_map.Set(kTestPolicy, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(list.Clone()), nullptr);
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  errors.Clear();

  // List with an int is OK, but error is added.
  list.Append(175);  // hex af, 255's sake.
  policy_map.Set(kTestPolicy, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(list.Clone()), nullptr);
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  list.clear();
  errors.Clear();

  // List with a string is OK.
  list.Append("any_string");
  policy_map.Set(kTestPolicy, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(list.Clone()), nullptr);
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  errors.Clear();
}

TEST(StringListPolicyHandlerTest, ApplyPolicySettings) {
  base::Value::List list;
  base::Value::List expected;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value;
  StringListPolicyHandler handler(kTestPolicy, kTestPref);

  // Empty list applies as empty list.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);

  // List with any string applies that string.
  list.Append("any_string");
  expected.Append("any_string");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);
  list.clear();
  expected.clear();

  // List with a string and an integer filters out the integer.
  list.Append("any_string");
  list.Append(42);
  expected.Append("any_string");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);
  list.clear();
  expected.clear();
}

TEST(StringToIntEnumListPolicyHandlerTest, CheckPolicySettings) {
  base::Value::List list;
  PolicyMap policy_map;
  PolicyErrorMap errors;
  StringMappingListPolicyHandler handler(
      kTestPolicy, kTestPref, base::BindRepeating(GetIntegerTypeMap));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("one");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("invalid");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(kTestPolicy).empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("no list"), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(kTestPolicy).empty());
}

TEST(StringMappingListPolicyHandlerTest, ApplyPolicySettings) {
  base::Value::List list;
  base::Value::List expected;
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value;
  StringMappingListPolicyHandler handler(
      kTestPolicy, kTestPref, base::BindRepeating(GetIntegerTypeMap));

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);

  list.Append("two");
  expected.Append(2);
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);

  list.Append("invalid");
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(list.Clone()), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);
}

TEST(IntRangePolicyHandler, CheckPolicySettingsClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are not rejected
  // (because clamping is enabled) but do yield a warning message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(-5), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(15), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("invalid"), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntRangePolicyHandler, CheckPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, false);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are rejected and yield
  // an error message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(-5), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(15), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("invalid"), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntRangePolicyHandler, ApplyPolicySettingsClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  std::unique_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(5);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(10);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  // Check that values lying outside the accepted range are clamped and written
  // to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(-5), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(15), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(10);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);
}

TEST(IntRangePolicyHandler, ApplyPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  std::unique_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntRangePolicyHandler handler(kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(5);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(10);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);
}

TEST(IntPercentageToDoublePolicyHandler, CheckPolicySettingsClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are not rejected
  // (because clamping is enabled) but do yield a warning message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(-5), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(15), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("invalid"), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntPercentageToDoublePolicyHandler, CheckPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PolicyErrorMap errors;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      kTestPolicy, kTestPref, 0, 10, false);

  // Check that values lying in the accepted range are not rejected.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Check that values lying outside the accepted range are rejected and yield
  // an error message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(-5), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(15), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // Check that an entirely invalid value is rejected and yields an error
  // message.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("invalid"), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(IntPercentageToDoublePolicyHandler, ApplyPolicySettingsClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  std::unique_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.0);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.05);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.1);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  // Check that values lying outside the accepted range are clamped and written
  // to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(-5), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.0);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(15), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.1);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);
}

TEST(IntPercentageToDoublePolicyHandler, ApplyPolicySettingsDontClamp) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  std::unique_ptr<base::Value> expected;
  const base::Value* value;

  // This tests needs to modify an int policy. The exact policy used and its
  // semantics outside the test are irrelevant.
  IntPercentageToDoublePolicyHandler handler(
      kTestPolicy, kTestPref, 0, 10, true);

  // Check that values lying in the accepted range are written to the pref.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.0);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(5), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.05);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(10), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  expected = std::make_unique<base::Value>(0.1);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);
}

TEST(SchemaValidatingPolicyHandlerTest, CheckAndGetValueInvalid) {
  std::string error;
  static const char kSchemaJson[] =
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"OneToThree\": {"
      "      \"type\": \"integer\","
      "      \"minimum\": 1,"
      "      \"maximum\": 3"
      "    },"
      "    \"Colors\": {"
      "      \"type\": \"string\","
      "      \"enum\": [ \"Red\", \"Green\", \"Blue\" ]"
      "    }"
      "  }"
      "}";
  Schema schema = Schema::Parse(kSchemaJson, &error);
  ASSERT_TRUE(schema.valid()) << error;

  static const char kPolicyMapJson[] =
      "{"
      "  \"PolicyForTesting\": {"
      "    \"OneToThree\": 2,"
      "    \"Colors\": \"White\""
      "  }"
      "}";
  auto parsed_json_or_error = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJson, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json_or_error.has_value())
      << parsed_json_or_error.error().message;

  base::Value& parsed_json = *parsed_json_or_error;
  ASSERT_TRUE(parsed_json.is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json.GetDict(), POLICY_LEVEL_RECOMMENDED,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  TestSchemaValidatingPolicyHandler handler(schema, SCHEMA_ALLOW_UNKNOWN);
  std::unique_ptr<base::Value> output_value;
  EXPECT_FALSE(handler.CheckAndGetValueForTest(policy_map, /*errors=*/nullptr,
                                               &output_value));
}

TEST(SchemaValidatingPolicyHandlerTest, CheckAndGetValueUnknown) {
  std::string error;
  static const char kSchemaJson[] =
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"OneToThree\": {"
      "      \"type\": \"integer\","
      "      \"minimum\": 1,"
      "      \"maximum\": 3"
      "    },"
      "    \"Colors\": {"
      "      \"type\": \"string\","
      "      \"enum\": [ \"Red\", \"Green\", \"Blue\" ]"
      "    }"
      "  }"
      "}";
  Schema schema = Schema::Parse(kSchemaJson, &error);
  ASSERT_TRUE(schema.valid()) << error;

  static const char kPolicyMapJson[] =
      "{"
      "  \"PolicyForTesting\": {"
      "    \"OneToThree\": 2,"
      "    \"Apples\": \"Red\""
      "  }"
      "}";
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJson, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_RECOMMENDED,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  TestSchemaValidatingPolicyHandler handler(schema, SCHEMA_ALLOW_UNKNOWN);

  // Test that CheckPolicySettings() is true but outputs warnings about unknown
  // properties.
  PolicyErrorMap error_map;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &error_map));
  EXPECT_THAT(error_map.GetErrors(kPolicyName),
              testing::ElementsAre(testing::FieldsAre(
                  testing::_, PolicyMap::MessageType::kWarning)));
  error_map.Clear();

  std::unique_ptr<base::Value> output_value;
  ASSERT_TRUE(
      handler.CheckAndGetValueForTest(policy_map, &error_map, &output_value));
  ASSERT_TRUE(output_value);
  ASSERT_TRUE(output_value->is_dict());
  const base::Value::Dict& output = output_value->GetDict();

  // Test that CheckAndGetValue outputs warnings about unknown properties.
  EXPECT_THAT(error_map.GetErrors(kPolicyName),
              testing::ElementsAre(testing::FieldsAre(
                  testing::_, PolicyMap::MessageType::kWarning)));

  // Test that CheckAndGetValue() actually dropped unknown properties.
  const absl::optional<int> one_two_three = output.FindInt("OneToThree");
  ASSERT_TRUE(one_two_three);
  int int_value = one_two_three.value();
  EXPECT_EQ(2, int_value);
  EXPECT_FALSE(output.contains("Apples"));
}

TEST(SimpleSchemaValidatingPolicyHandlerTest, CheckAndGetValue) {
  static const char kSchemaJson[] =
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"PolicyForTesting\": {"
      "      \"type\": \"object\","
      "      \"properties\": {"
      "        \"OneToThree\": {"
      "          \"type\": \"integer\","
      "          \"minimum\": 1,"
      "          \"maximum\": 3"
      "        },"
      "        \"Colors\": {"
      "          \"type\": \"string\","
      "          \"enum\": [ \"Red\", \"Green\", \"Blue\" ]"
      "        }"
      "      }"
      "    }"
      "  }"
      "}";
  std::string error;
  Schema schema = Schema::Parse(kSchemaJson, &error);
  ASSERT_TRUE(schema.valid()) << error;

  static const char kPolicyMapJson[] =
      "{"
      "  \"PolicyForTesting\": {"
      "    \"OneToThree\": 2,"
      "    \"Colors\": \"Green\""
      "  }"
      "}";
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJson, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map_recommended;
  policy_map_recommended.LoadFrom(parsed_json->GetDict(),
                                  POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_CLOUD);

  PolicyMap policy_map_mandatory;
  policy_map_mandatory.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_MANDATORY,
                                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  SimpleSchemaValidatingPolicyHandler handler_all(
      kPolicyName, kTestPref, schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED);

  SimpleSchemaValidatingPolicyHandler handler_recommended(
      kPolicyName, kTestPref, schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED);

  SimpleSchemaValidatingPolicyHandler handler_mandatory(
      kPolicyName, kTestPref, schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED);

  SimpleSchemaValidatingPolicyHandler handler_none(
      kPolicyName, kTestPref, schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED);

  const base::Value* value_expected_in_pref =
      parsed_json->FindPath(kPolicyName);

  PolicyErrorMap errors;
  PrefValueMap prefs;
  base::Value* value_set_in_pref;

  EXPECT_TRUE(handler_all.CheckPolicySettings(policy_map_mandatory, &errors));
  EXPECT_TRUE(errors.empty());
  prefs.Clear();
  handler_all.ApplyPolicySettings(policy_map_mandatory, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);

  EXPECT_FALSE(
      handler_recommended.CheckPolicySettings(policy_map_mandatory, &errors));
  EXPECT_FALSE(errors.empty());
  errors.Clear();

  EXPECT_TRUE(
      handler_mandatory.CheckPolicySettings(policy_map_mandatory, &errors));
  EXPECT_TRUE(errors.empty());
  prefs.Clear();
  handler_mandatory.ApplyPolicySettings(policy_map_mandatory, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);

  EXPECT_FALSE(handler_none.CheckPolicySettings(policy_map_mandatory, &errors));
  EXPECT_FALSE(errors.empty());
  errors.Clear();

  EXPECT_TRUE(handler_all.CheckPolicySettings(policy_map_recommended, &errors));
  EXPECT_TRUE(errors.empty());
  prefs.Clear();
  handler_all.ApplyPolicySettings(policy_map_mandatory, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);

  EXPECT_FALSE(
      handler_mandatory.CheckPolicySettings(policy_map_recommended, &errors));
  EXPECT_FALSE(errors.empty());
  errors.Clear();

  EXPECT_TRUE(
      handler_recommended.CheckPolicySettings(policy_map_recommended, &errors));
  EXPECT_TRUE(errors.empty());
  prefs.Clear();
  handler_recommended.ApplyPolicySettings(policy_map_mandatory, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);

  EXPECT_FALSE(
      handler_none.CheckPolicySettings(policy_map_recommended, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(SimpleJsonStringSchemaValidatingPolicyHandlerTest, ValidEmbeddedJson) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJsonValid, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  const base::Value* value_expected_in_pref =
      parsed_json->FindPath(kPolicyName);

  PolicyErrorMap errors;
  PrefValueMap prefs;
  base::Value* value_set_in_pref;

  // This value matches the schema - handler shouldn't record any errors.
  std::unique_ptr<SimpleJsonStringSchemaValidatingPolicyHandler> handler =
      JsonStringHandlerForTesting();
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  handler->ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);
}

TEST(SimpleJsonStringSchemaValidatingPolicyHandlerTest, InvalidEmbeddedJson) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJsonInvalid, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  const base::Value* value_expected_in_pref =
      parsed_json->FindPath(kPolicyName);

  PolicyErrorMap errors;
  PrefValueMap prefs;
  base::Value* value_set_in_pref;

  // Handler accepts JSON that doesn't match the schema, but records errors.
  std::unique_ptr<SimpleJsonStringSchemaValidatingPolicyHandler> handler =
      JsonStringHandlerForTesting();
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  handler->ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);
}

TEST(SimpleJsonStringSchemaValidatingPolicyHandlerTest, UnparsableJson) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJsonUnparsable, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  const base::Value* value_expected_in_pref =
      parsed_json->FindPath(kPolicyName);

  PolicyErrorMap errors;
  PrefValueMap prefs;
  base::Value* value_set_in_pref;

  // Handler accepts unparsable JSON, but records errors.
  std::unique_ptr<SimpleJsonStringSchemaValidatingPolicyHandler> handler =
      JsonStringHandlerForTesting();
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  handler->ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);
}

TEST(SimpleJsonStringSchemaValidatingPolicyHandlerTest, WrongType) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJsonWrongTypes, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  const base::Value* value_expected_in_pref =
      parsed_json->FindPath(kPolicyName);

  PolicyErrorMap errors;
  PrefValueMap prefs;
  base::Value* value_set_in_pref;

  // Handler allows wrong types (not at the root), but records errors.
  std::unique_ptr<SimpleJsonStringSchemaValidatingPolicyHandler> handler =
      JsonStringHandlerForTesting();
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  handler->ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));
  EXPECT_EQ(*value_expected_in_pref, *value_set_in_pref);
}

TEST(SimpleJsonStringSchemaValidatingPolicyHandlerTest, WrongRootType) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPolicyMapJsonWrongRootType, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ASSERT_TRUE(parsed_json->is_dict());

  PolicyMap policy_map;
  policy_map.LoadFrom(parsed_json->GetDict(), POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD);

  PolicyErrorMap errors;

  // Handler rejects the wrong root type and records errors.
  std::unique_ptr<SimpleJsonStringSchemaValidatingPolicyHandler> handler =
      JsonStringHandlerForTesting();
  EXPECT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}

TEST(SimpleDeprecatingPolicyHandlerTest, CheckDeprecatedUsedWhenNoNewValue) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  std::unique_ptr<base::Value> expected;
  const base::Value* value;
  PolicyErrorMap errors;
  PolicyHandlerParameters params;
  const char kLegacyPolicy[] = "legacy_policy";

  SimpleDeprecatingPolicyHandler handler(
      std::make_unique<SimplePolicyHandler>(kLegacyPolicy, kTestPref,
                                            base::Value::Type::INTEGER),
      std::make_unique<SimplePolicyHandler>(kTestPolicy, kTestPref,
                                            base::Value::Type::INTEGER));

  // Check that legacy value alone is used.
  policy_map.Set(kLegacyPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(42), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  prefs.Clear();
  handler.ApplyPolicySettingsWithParameters(policy_map, params, &prefs);
  expected = std::make_unique<base::Value>(42);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  // Set the new value as invalid and verify that the total result is invalid.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("0"), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  prefs.Clear();

  // Set the new value and verify that it overrides the legacy.
  policy_map.Set(kTestPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(1337), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  prefs.Clear();
  handler.ApplyPolicySettingsWithParameters(policy_map, params, &prefs);
  expected = std::make_unique<base::Value>(1337);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);

  // Erasing the legacy value should have no effect at this point.
  policy_map.Erase(kLegacyPolicy);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  prefs.Clear();
  handler.ApplyPolicySettingsWithParameters(policy_map, params, &prefs);
  expected = std::make_unique<base::Value>(1337);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(*expected, *value);
}

}  // namespace policy
