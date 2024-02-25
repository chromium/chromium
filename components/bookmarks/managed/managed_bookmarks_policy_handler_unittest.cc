// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmarks_policy_handler.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace bookmarks {

using policy::POLICY_LEVEL_MANDATORY;
using policy::POLICY_SCOPE_USER;
using policy::POLICY_SOURCE_CLOUD;
using policy::PolicyMap;
using policy::Schema;
using policy::key::kManagedBookmarks;

class ManagedBookmarksPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(policy::GetChromeSchemaData());
    handler_list_.AddHandler(
        base::WrapUnique<policy::ConfigurationPolicyHandler>(
            new ManagedBookmarksPolicyHandler(chrome_schema)));
  }
};

TEST_F(ManagedBookmarksPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(store_->GetValue(prefs::kManagedBookmarks, nullptr));

  PolicyMap policy;
  policy.Set(kManagedBookmarks, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::JSONReader::Read("["
                                    // The following gets filtered out from
                                    // the JSON string when parsed.
                                    "  {"
                                    "    \"toplevel_name\": \"abc 123\""
                                    "  },"
                                    "  {"
                                    "    \"name\": \"Google\","
                                    "    \"url\": \"google.com\""
                                    "  },"
                                    "  {"
                                    "    \"name\": \"Empty Folder\","
                                    "    \"children\": []"
                                    "  },"
                                    "  {"
                                    "    \"name\": \"Big Folder\","
                                    "    \"children\": ["
                                    "      {"
                                    "        \"name\": \"Youtube\","
                                    "        \"url\": \"youtube.com\""
                                    "      },"
                                    "      {"
                                    "        \"name\": \"Chromium\","
                                    "        \"url\": \"chromium.org\""
                                    "      },"
                                    "      {"
                                    "        \"name\": \"More Stuff\","
                                    "        \"children\": ["
                                    "          {"
                                    "            \"name\": \"Bugs\","
                                    "            \"url\": \"crbug.com\""
                                    "          }"
                                    "        ]"
                                    "      }"
                                    "    ]"
                                    "  }"
                                    "]"),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedBookmarks, &pref_value));
  ASSERT_TRUE(pref_value);

  // Make sure the kManagedBookmarksFolderName pref is set correctly.
  const base::Value* folder_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedBookmarksFolderName, &folder_value));
  ASSERT_TRUE(folder_value);
  ASSERT_TRUE(folder_value->is_string());
  EXPECT_EQ("abc 123", folder_value->GetString());

  // Note the protocols and ending slashes added to urls, which were not in the
  // value set earlier.
  std::optional<base::Value> expected = base::JSONReader::Read(R"(
    [
      {
        "name": "Google",
        "url": "http://google.com/"
      },
      {
        "name": "Empty Folder",
        "children": []
      },
      {
        "name": "Big Folder",
        "children": [
          {
            "name": "Youtube",
            "url": "http://youtube.com/"
          },
          {
            "name": "Chromium",
            "url": "http://chromium.org/"
          },
          {
            "name": "More Stuff",
            "children": [
              {
                "name": "Bugs",
                "url": "http://crbug.com/"
              }
            ]
          }
        ]
      }
    ]
  )");
  EXPECT_EQ(expected, *pref_value);
}

TEST_F(ManagedBookmarksPolicyHandlerTest, ApplyPolicySettingsNoTitle) {
  EXPECT_FALSE(store_->GetValue(prefs::kManagedBookmarks, nullptr));

  PolicyMap policy;
  policy.Set(kManagedBookmarks, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::JSONReader::Read("["
                                    "  {"
                                    "    \"name\": \"Google\","
                                    "    \"url\": \"google.com\""
                                    "  }"
                                    "]"),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedBookmarks, &pref_value));
  ASSERT_TRUE(pref_value);

  // Make sure the kManagedBookmarksFolderName pref is set correctly.
  const base::Value* folder_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kManagedBookmarksFolderName, &folder_value));
  ASSERT_TRUE(folder_value);
  ASSERT_TRUE(folder_value->is_string());
  EXPECT_EQ("", folder_value->GetString());

  // Note the protocol and ending slash added to url, which was not in the value
  // set earlier.
  std::optional<base::Value> expected = base::JSONReader::Read(R"(
    [
      {
        "name": "Google",
        "url": "http://google.com/"
      }
    ]
  )");
  EXPECT_EQ(expected, *pref_value);
}

TEST_F(ManagedBookmarksPolicyHandlerTest, WrongPolicyType) {
  PolicyMap policy;
  // The expected type is a list base::Value, but this policy sets it as an
  // unparsed base::Value. Any type other than list should fail.
  policy.Set(kManagedBookmarks, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value("["
                         "  {"
                         "    \"name\": \"Google\","
                         "    \"url\": \"google.com\""
                         "  },"
                         "]"),
             nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(store_->GetValue(prefs::kManagedBookmarks, nullptr));
}

TEST_F(ManagedBookmarksPolicyHandlerTest, UnknownKeys) {
  PolicyMap policy;
  policy.Set(kManagedBookmarks, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::JSONReader::Read("["
                                    "  {"
                                    "    \"name\": \"Google\","
                                    "    \"unknown\": \"should be ignored\","
                                    "    \"url\": \"google.com\""
                                    "  }"
                                    "]"),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedBookmarks, &pref_value));
  ASSERT_TRUE(pref_value);

  // Note the protocol and ending slash added to url, which was not in the value
  // set earlier.
  std::optional<base::Value> expected = base::JSONReader::Read(R"(
    [
      {
        "name": "Google",
        "url": "http://google.com/"
      }
    ]
  )");
  EXPECT_EQ(expected, *pref_value);
}

TEST_F(ManagedBookmarksPolicyHandlerTest, BadBookmark) {
  PolicyMap policy;
  policy.Set(kManagedBookmarks, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD,
             base::JSONReader::Read("["
                                    "  {"
                                    "    \"name\": \"Empty\","
                                    "    \"url\": \"\""
                                    "  },"
                                    "  {"
                                    "    \"name\": \"Invalid type\","
                                    "    \"url\": 4"
                                    "  },"
                                    "  {"
                                    "    \"name\": \"Invalid URL\","
                                    "    \"url\": \"?\""
                                    "  },"
                                    "  {"
                                    "    \"name\": \"Google\","
                                    "    \"url\": \"google.com\""
                                    "  }"
                                    "]"),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  // Invalid because SCHEMA_ALLOW_INVALID was replaced by SCHEMA_ALLOW_UNKNOWN
  // which has stricter verification rules (see https://www.crbug/969706)
  EXPECT_FALSE(store_->GetValue(prefs::kManagedBookmarks, &pref_value));
  ASSERT_FALSE(pref_value);
}

}  // namespace bookmarks
