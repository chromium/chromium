// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/data_controls_policy_handler.h"

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/values.h"
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
                    "FILE_ATTACH",
                    "FILE_DOWNLOAD",
                    "PRINTING"
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
              "byte_size_higher_than": {
                "minimum": 0,
                "type": "integer"
              },
              "byte_size_lower_than": {
                "minimum": 0,
                "type": "integer"
              },
              "file_number_higher_than": {
                "minimum": 0,
                "type": "integer"
              },
              "file_number_lower_than": {
                "minimum": 0,
                "type": "integer"
              },
              "file_type": {
                "items": {
                  "type": "string"
                },
                "type": "array"
              },
              "incognito": {
                "type": "boolean"
              },
              "mime_type": {
                "items": {
                  "type": "string"
                },
                "type": "array"
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
      }
    },
    {
      "sources": {
        "urls": ["https://foo.com"],
        "incognito": false,
      }
    }
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
            }
          ])",
        u"Error at PolicyForTesting[0]: Keys \"and, or\" cannot be set in the "
        u"same dictionary"},

    {
        R"([
            {
              "not": {},
              "destinations": {}
            }
          ])",
        u"Error at PolicyForTesting[0]: Keys \"destinations\" cannot be set in "
        u"the same dictionary as the \"not\" keys"},
    {
        R"([
            {
              "sources": {
                "os_clipboard": true,
                "incognito": true
              }
            }
          ])",
        u"Error at PolicyForTesting[0].sources: Keys \"incognito\" cannot be "
        u"set in the same dictionary as the \"os_clipboard\" keys"}};

class DataControlsPolicyHandlerTest : public testing::Test {
 public:
  policy::Schema schema() {
    std::string error;
    policy::Schema validation_schema = policy::Schema::Parse(kSchema, &error);
    EXPECT_TRUE(error.empty());
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

TEST_F(DataControlsPolicyHandlerTest, BlocksInvalidSchema) {
  for (auto scope : kCloudSources) {
    policy::PolicyMap map = CreatePolicyMap(kInvalidPolicy, scope);
    auto handler = std::make_unique<DataControlsPolicyHandler>(
        kPolicyName, kTestPref, schema());

    policy::PolicyErrorMap errors;
    ASSERT_FALSE(handler->CheckPolicySettings(map, &errors));
    ASSERT_FALSE(errors.empty());
    ASSERT_TRUE(errors.HasError(kPolicyName));
    std::u16string messages = errors.GetErrorMessages(kPolicyName);
    ASSERT_EQ(messages,
              u"Error at PolicyForTesting[0]: Schema validation error: Policy "
              u"type mismatch: expected: \"dictionary\", actual: \"integer\".");
  }
}

TEST_P(DataControlsPolicyHandlerInvalidKeysTest, Test) {
  policy::PolicyMap map = CreatePolicyMap(
      policy_value(), policy::PolicySource::POLICY_SOURCE_CLOUD);
  auto handler = std::make_unique<DataControlsPolicyHandler>(
      kPolicyName, kTestPref, schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(map, &errors));
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.HasError(kPolicyName));
  std::u16string messages = errors.GetErrorMessages(kPolicyName);
  ASSERT_EQ(messages, expected_messages());
}

}  // namespace data_controls
