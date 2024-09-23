// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/data_controls_policy_handler.h"

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr char kTestPref[] = "data_controls.test_pref";

constexpr char kPolicyName[] = "PolicyForTesting";

// Schema copied from out/Default/gen/chrome/app/policy/policy_templates.json
constexpr char kSchema[] = R"(
{
  "type": "object",
  "properties": {
    "PolicyForTesting": {
      "items": {
        "properties": {
          "and": {
            "items": {
              "id": "DataControlsCondition",
              "properties": {
                "and": {
                  "items": {
                    "$ref": "DataControlsCondition"
                  },
                  "type": "array"
                },
                "destinations": {
                  "properties": {
                    "incognito": {
                      "type": "boolean"
                    },
                    "os_clipboard": {
                      "type": "boolean"
                    },
                    "other_profile": {
                      "type": "boolean"
                    },
                    "urls": {
                      "items": {
                        "type": "string"
                      },
                      "type": "array"
                    }
                  },
                  "type": "object"
                },
                "not": {
                  "$ref": "DataControlsCondition"
                },
                "or": {
                  "items": {
                    "$ref": "DataControlsCondition"
                  },
                  "type": "array"
                },
                "sources": {
                  "properties": {
                    "incognito": {
                      "type": "boolean"
                    },
                    "os_clipboard": {
                      "type": "boolean"
                    },
                    "other_profile": {
                      "type": "boolean"
                    },
                    "urls": {
                      "items": {
                        "type": "string"
                      },
                      "type": "array"
                    }
                  },
                  "type": "object"
                }
              },
              "type": "object"
            },
            "type": "array"
          },
          "description": {
            "type": "string"
          },
          "destinations": {
            "properties": {
              "incognito": {
                "type": "boolean"
              },
              "os_clipboard": {
                "type": "boolean"
              },
              "other_profile": {
                "type": "boolean"
              },
              "urls": {
                "items": {
                  "type": "string"
                },
                "type": "array"
              }
            },
            "type": "object"
          },
          "name": {
            "type": "string"
          },
          "not": {
            "properties": {
              "and": {
                "items": {
                  "$ref": "DataControlsCondition"
                },
                "type": "array"
              },
              "destinations": {
                "properties": {
                  "incognito": {
                    "type": "boolean"
                  },
                  "os_clipboard": {
                    "type": "boolean"
                  },
                  "other_profile": {
                    "type": "boolean"
                  },
                  "urls": {
                    "items": {
                      "type": "string"
                    },
                    "type": "array"
                  }
                },
                "type": "object"
              },
              "not": {
                "$ref": "DataControlsCondition"
              },
              "or": {
                "items": {
                  "$ref": "DataControlsCondition"
                },
                "type": "array"
              },
              "sources": {
                "properties": {
                  "incognito": {
                    "type": "boolean"
                  },
                  "os_clipboard": {
                    "type": "boolean"
                  },
                  "other_profile": {
                    "type": "boolean"
                  },
                  "urls": {
                    "items": {
                      "type": "string"
                    },
                    "type": "array"
                  }
                },
                "type": "object"
              }
            },
            "type": "object"
          },
          "or": {
            "items": {
              "properties": {
                "and": {
                  "items": {
                    "$ref": "DataControlsCondition"
                  },
                  "type": "array"
                },
                "destinations": {
                  "properties": {
                    "incognito": {
                      "type": "boolean"
                    },
                    "os_clipboard": {
                      "type": "boolean"
                    },
                    "other_profile": {
                      "type": "boolean"
                    },
                    "urls": {
                      "items": {
                        "type": "string"
                      },
                      "type": "array"
                    }
                  },
                  "type": "object"
                },
                "not": {
                  "$ref": "DataControlsCondition"
                },
                "or": {
                  "items": {
                    "$ref": "DataControlsCondition"
                  },
                  "type": "array"
                },
                "sources": {
                  "properties": {
                    "incognito": {
                      "type": "boolean"
                    },
                    "os_clipboard": {
                      "type": "boolean"
                    },
                    "other_profile": {
                      "type": "boolean"
                    },
                    "urls": {
                      "items": {
                        "type": "string"
                      },
                      "type": "array"
                    }
                  },
                  "type": "object"
                }
              },
              "type": "object"
            },
            "type": "array"
          },
          "restrictions": {
            "items": {
              "properties": {
                "class": {
                  "enum": [
                    "CLIPBOARD",
                    "SCREENSHOT"
                  ],
                  "type": "string"
                },
                "level": {
                  "enum": [
                    "BLOCK",
                    "WARN",
                    "REPORT"
                  ],
                  "type": "string"
                }
              },
              "type": "object"
            },
            "type": "array"
          },
          "rule_id": {
            "type": "string"
          },
          "sources": {
            "properties": {
              "incognito": {
                "type": "boolean"
              },
              "os_clipboard": {
                "type": "boolean"
              },
              "other_profile": {
                "type": "boolean"
              },
              "urls": {
                "items": {
                  "type": "string"
                },
                "type": "array"
              }
            },
            "type": "object"
          }
        },
        "type": "object"
      },
      "type": "array"
    }
  }
})";

constexpr char kValidPolicy[] = R"(
  [
    {
      "destinations": {
        "urls": ["https://google.com"],
        "incognito": true,
      },
      "restrictions": [
        {
          "class": "CLIPBOARD",
          "level": "BLOCK"
        }
      ]
    },
    {
      "sources": {
        "urls": ["https://foo.com"],
        "incognito": false,
      },
      "restrictions": [
        {
          "class": "CLIPBOARD",
          "level": "WARN"
        }
      ]
    }
  ]
)";

// Only the first entry is valid, the second one is not a dict, the third one
// has an invalid condition and the fourth one has an unsupported restriction.
constexpr char kPartiallyValidPolicy[] = R"(
  [
    {
      "destinations": {
        "urls": ["https://google.com"],
        "incognito": true,
      },
      "restrictions": [
        {
          "class": "CLIPBOARD",
          "level": "BLOCK"
        }
      ]
    },
    1234,
    {
      "and": [],
      "or": [],
      "restrictions": [
        {
          "class": "CLIPBOARD",
          "level": "BLOCK"
        }
      ]
    },
    {
      "sources": {
        "urls": ["https://google.com"],
        "incognito": true,
      },
      "restrictions": [
        {
          "class": "PRINT",
          "level": "BLOCK"
        }
      ]
    },
  ]
)";

constexpr policy::PolicySource kCloudSources[] = {
    policy::PolicySource::POLICY_SOURCE_CLOUD,
    policy::PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH};

constexpr policy::PolicySource kNonCloudSources[] = {
    policy::PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
    policy::PolicySource::POLICY_SOURCE_COMMAND_LINE,
    policy::PolicySource::POLICY_SOURCE_ACTIVE_DIRECTORY,
    policy::PolicySource::POLICY_SOURCE_PLATFORM,
    policy::PolicySource::
        POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE,
};

constexpr char kInvalidPolicy[] = "[1,2,3]";

constexpr std::pair<const char*, const char16_t*> kInvalidTestCases[] = {
    {
        R"([
            {
              "or": [],
              "and": [],
              "restrictions": [
                {
                  "class": "SCREENSHOT",
                  "level": "BLOCK"
                }
              ]
            }
          ])",
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
        u"Error at PolicyForTesting[0]: Keys \"and, or\" cannot be set in the "
        u"same dictionary",
#else
        u"Error at PolicyForTesting[0]: \"SCREENSHOT\" is not a supported "
        u"restriction on this platform",
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    },

    {
        R"([
            {
              "not": {},
              "destinations": {},
              "restrictions": [
                {
                  "class": "SCREENSHOT",
                  "level": "BLOCK"
                }
              ]
            }
          ])",
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
        u"Error at PolicyForTesting[0]: Keys \"destinations\" cannot be set in "
        u"the same dictionary as the \"not\" keys",
#else
        u"Error at PolicyForTesting[0]: \"SCREENSHOT\" is not a supported "
        u"restriction on this platform",
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    },
    {
        R"([
            {
              "sources": {
                "os_clipboard": true,
                "incognito": true
              },
              "restrictions": [
                {
                  "class": "SCREENSHOT",
                  "level": "BLOCK"
                }
              ]
            }
          ])",
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
        u"Error at PolicyForTesting[0].sources: Keys \"incognito\" cannot be "
        u"set in the same dictionary as the \"os_clipboard\" keys",
#else
        u"Error at PolicyForTesting[0]: \"SCREENSHOT\" is not a supported "
        u"restriction on this platform",
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    },
    {
        R"([
             {
               "destinations": { "urls": [ "google.com" ] },
               "restrictions": [
                 {
                   "class": "SCREENSHOT",
                   "level": "BLOCK"
                 }
               ]
             }
          ])",
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
        u"Error at PolicyForTesting[0]: \"destinations\" is not a supported "
        u"condition for \"SCREENSHOT\"",
#else
        u"Error at PolicyForTesting[0]: \"SCREENSHOT\" is not a supported "
        u"restriction on this platform",
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    },
    {
        R"([
             {
               "sources": { "urls": [ "google.com" ] },
               "restrictions": [
                 {
                   "class": "SCREENSHOT",
                   "level": "WARN"
                 }
               ]
             }
          ])",
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
        u"Error at PolicyForTesting[0]: \"SCREENSHOT\" cannot be set to "
        u"\"WARN\"",
#else
        u"Error at PolicyForTesting[0]: \"SCREENSHOT\" is not a supported "
        u"restriction on this platform",
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    },
    {
        R"([
             {
               "restrictions": [
                 {
                   "class": "PRINTING",
                   "level": "WARN"
                 }
               ]
             }
          ])",
        u"Error at PolicyForTesting[0].restrictions[0].class: Schema "
        u"validation error: Invalid value for string\n"
        u"Error at PolicyForTesting[0]: \"PRINTING\" is not a supported "
        u"restriction on this platform",
    },
};

class DataControlsPolicyHandlerTest : public testing::Test {
 public:
  policy::Schema schema() {
    ASSIGN_OR_RETURN(const auto validation_schema,
                     policy::Schema::Parse(kSchema), [](const auto& e) {
                       ADD_FAILURE() << e;
                       return policy::Schema();
                     });
    return validation_schema;
  }

  policy::PolicyMap CreatePolicyMap(const std::string& policy,
                                    policy::PolicySource policy_source) {
    policy::PolicyMap policy_map;

    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_MACHINE, policy_source,
                   policy_value(policy), nullptr);

    return policy_map;
  }

  std::optional<base::Value> policy_value(const std::string& policy) const {
    return base::JSONReader::Read(policy, base::JSON_ALLOW_TRAILING_COMMAS);
  }
};

// Tests polices with a valid schema, but invalid key usages.
class DataControlsPolicyHandlerInvalidKeysTest
    : public testing::WithParamInterface<
          std::pair<const char*, const char16_t*>>,
      public DataControlsPolicyHandlerTest {
 public:
  const char* policy_value() { return GetParam().first; }
  const char16_t* expected_messages() { return GetParam().second; }
};

INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsPolicyHandlerInvalidKeysTest,
                         testing::ValuesIn(kInvalidTestCases));

}  // namespace

TEST_F(DataControlsPolicyHandlerTest, AllowsCloudSources) {
  for (auto scope : kCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kValidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));
    ASSERT_TRUE(errors.empty());

    PrefValueMap prefs;
    base::Value* value_set_in_pref;
    handler->ApplyPolicySettings(map, &prefs);

    ASSERT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));

    auto* value_set_in_map = map.GetValueUnsafe(kPolicyName);
    ASSERT_TRUE(value_set_in_map);
    ASSERT_EQ(*value_set_in_map, *value_set_in_pref);
  }
}

TEST_F(DataControlsPolicyHandlerTest, BlocksNonCloudSources) {
  for (auto scope : kNonCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kValidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_FALSE(handler->CheckPolicySettings(map, &errors));
    ASSERT_FALSE(errors.empty());
    ASSERT_TRUE(errors.HasError(kPolicyName));
    std::u16string messages = errors.GetErrorMessages(kPolicyName);
    ASSERT_EQ(messages,
              u"Ignored because the policy is not set by a cloud source.");
  }
}

TEST_F(DataControlsPolicyHandlerTest, WarnInvalidSchema) {
  for (auto scope : kCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kInvalidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;

    // Invalid list entries are tolerated, so in that case `CheckPolicySettings`
    // will still return true.
    ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));

    ASSERT_FALSE(errors.empty());
    ASSERT_TRUE(errors.HasError(kPolicyName));
    std::u16string messages = errors.GetErrorMessages(kPolicyName);
    ASSERT_EQ(messages,
              u"Error at PolicyForTesting[2]: Schema validation error: Policy "
              u"type mismatch: expected: \"dictionary\", actual: \"integer\".");
  }
}

TEST_F(DataControlsPolicyHandlerTest, AllowsPartiallyValidRules) {
  for (auto scope : kCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kPartiallyValidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));
    ASSERT_FALSE(errors.empty());
    ASSERT_TRUE(errors.HasError(kPolicyName));
    std::u16string messages = errors.GetErrorMessages(kPolicyName);
    ASSERT_EQ(
        messages,
        u"Error at PolicyForTesting[3].restrictions[0].class: Schema "
        u"validation error: Invalid value for string\n"
        u"Error at PolicyForTesting[2]: Keys \"and, or\" cannot be set in the "
        u"same dictionary");

    PrefValueMap prefs;
    base::Value* value_set_in_pref;
    handler->ApplyPolicySettings(map, &prefs);

    ASSERT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));

    // Only the valid rule in `kPartiallyValidPolicy` should have been applied.
    ASSERT_EQ(*base::JSONReader::Read(
                  R"(
                  [
                    {
                      "destinations": {
                        "urls": ["https://google.com"],
                        "incognito": true,
                      },
                      "restrictions": [
                        {
                          "class": "CLIPBOARD",
                          "level": "BLOCK"
                        }
                      ]
                    }
                  ])",
                  base::JSON_ALLOW_TRAILING_COMMAS),
              *value_set_in_pref);
  }
}

TEST_P(DataControlsPolicyHandlerInvalidKeysTest, Test) {
  policy::PolicyMap map = CreatePolicyMap(
      policy_value(), policy::PolicySource::POLICY_SOURCE_CLOUD);
  auto handler = std::make_unique<DataControlsPolicyHandler>(
      kPolicyName, kTestPref, schema());

  policy::PolicyErrorMap errors;

  // Invalid list entries are tolerated, so in that case `CheckPolicySettings`
  // will still return true.
  ASSERT_TRUE(handler->CheckPolicySettings(map, &errors));

  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.HasError(kPolicyName));

  std::u16string messages = errors.GetErrorMessages(kPolicyName);
  ASSERT_EQ(messages, expected_messages());
}

}  // namespace data_controls
