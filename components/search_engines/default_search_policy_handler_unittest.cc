// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_policy_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"

namespace policy {

class DefaultSearchPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  DefaultSearchPolicyHandlerTest() {
    default_alternate_urls_.AppendString(
        "http://www.google.com/#q={searchTerms}");
    default_alternate_urls_.AppendString(
        "http://www.google.com/search#q={searchTerms}");
  }

  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new DefaultSearchPolicyHandler));
  }

 protected:
  static const char kSearchURL[];
  static const char kSuggestURL[];
  static const char kIconURL[];
  static const char kName[];
  static const char kKeyword[];
  static const char kReplacementKey[];
  static const char kImageURL[];
  static const char kImageParams[];
  static const char kNewTabURL[];
  static const char kFileSearchURL[];
  static const char kHostName[];

  // Build a default search policy by setting search-related keys in |policy| to
  // reasonable values. You can update any of the keys after calling this
  // method.
  void BuildDefaultSearchPolicy(PolicyMap* policy);

  base::ListValue default_alternate_urls_;
};

const char DefaultSearchPolicyHandlerTest::kSearchURL[] =
    "http://test.com/search?t={searchTerms}";
const char DefaultSearchPolicyHandlerTest::kSuggestURL[] =
    "http://test.com/sugg?={searchTerms}";
const char DefaultSearchPolicyHandlerTest::kIconURL[] =
    "http://test.com/icon.jpg";
const char DefaultSearchPolicyHandlerTest::kName[] =
    "MyName";
const char DefaultSearchPolicyHandlerTest::kKeyword[] =
    "MyKeyword";
const char DefaultSearchPolicyHandlerTest::kImageURL[] =
    "http://test.com/searchbyimage/upload";
const char DefaultSearchPolicyHandlerTest::kImageParams[] =
    "image_content=content,image_url=http://test.com/test.png";
const char DefaultSearchPolicyHandlerTest::kNewTabURL[] =
    "http://test.com/newtab";
const char DefaultSearchPolicyHandlerTest::kFileSearchURL[] =
    "file:///c:/path/to/search?t={searchTerms}";
const char DefaultSearchPolicyHandlerTest::kHostName[] = "test.com";

void DefaultSearchPolicyHandlerTest::
    BuildDefaultSearchPolicy(PolicyMap* policy) {
  base::ListValue* encodings = new base::ListValue();
  encodings->AppendString("UTF-16");
  encodings->AppendString("UTF-8");
  policy->Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(true), nullptr);
  policy->Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kSearchURL), nullptr);
  policy->Set(key::kDefaultSearchProviderName, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kName), nullptr);
  policy->Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kKeyword), nullptr);
  policy->Set(key::kDefaultSearchProviderSuggestURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kSuggestURL), nullptr);
  policy->Set(key::kDefaultSearchProviderIconURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kIconURL), nullptr);
  policy->Set(key::kDefaultSearchProviderEncodings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              base::WrapUnique(encodings), nullptr);
  policy->Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              default_alternate_urls_.CreateDeepCopy(), nullptr);
  policy->Set(key::kDefaultSearchProviderImageURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kImageURL), nullptr);
  policy->Set(key::kDefaultSearchProviderImageURLPostParams,
              POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kImageParams), nullptr);
  policy->Set(key::kDefaultSearchProviderNewTabURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              std::make_unique<base::Value>(kNewTabURL), nullptr);
}

// Checks that if the default search policy is missing, that no elements of the
// default search policy will be present.
TEST_F(DefaultSearchPolicyHandlerTest, MissingUrl) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  policy.Erase(key::kDefaultSearchProviderSearchURL);
  UpdateProviderPolicy(policy);

  const base::Value* temp = nullptr;
  EXPECT_FALSE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(DefaultSearchPolicyHandlerTest, Invalid) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  const char bad_search_url[] = "http://test.com/noSearchTerms";
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(bad_search_url), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* temp = nullptr;
  EXPECT_FALSE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
}

// Checks that if the default search policy has invalid type for elements,
// that no elements of the default search policy will be present in prefs.
TEST_F(DefaultSearchPolicyHandlerTest, InvalidType) {
  // List of policies defined in test policy.
  const char* kPolicyNamesToCheck[] = {
      key::kDefaultSearchProviderEnabled,
      key::kDefaultSearchProviderName,
      key::kDefaultSearchProviderKeyword,
      key::kDefaultSearchProviderSearchURL,
      key::kDefaultSearchProviderSuggestURL,
      key::kDefaultSearchProviderIconURL,
      key::kDefaultSearchProviderEncodings,
      key::kDefaultSearchProviderAlternateURLs,
      key::kDefaultSearchProviderImageURL,
      key::kDefaultSearchProviderNewTabURL,
      key::kDefaultSearchProviderImageURLPostParams};

  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);

  for (auto* policy_name : kPolicyNamesToCheck) {
    // Check that policy can be successfully applied first.
    UpdateProviderPolicy(policy);
    const base::Value* temp = nullptr;
    EXPECT_TRUE(store_->GetValue(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));

    auto old_value = base::WrapUnique(policy.GetValue(policy_name)->DeepCopy());
    // BinaryValue is not supported in any current default search policy params.
    // Try changing policy param to BinaryValue and check that policy becomes
    // invalid.
    policy.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(base::Value::Type::BINARY),
               nullptr);
    UpdateProviderPolicy(policy);

    EXPECT_FALSE(store_->GetValue(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp))
        << "Policy type check failed " << policy_name;
    // Return old value to policy map.
    policy.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::move(old_value), nullptr);
  }
}

// Checks that for a fully defined search policy, all elements have been
// read properly into the dictionary pref.
TEST_F(DefaultSearchPolicyHandlerTest, FullyDefined) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  UpdateProviderPolicy(policy);

  const base::Value* temp = nullptr;
  const base::DictionaryValue* dictionary;
  std::string value;
  const base::ListValue* list_value;
  EXPECT_TRUE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
  temp->GetAsDictionary(&dictionary);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kURL, &value));
  EXPECT_EQ(kSearchURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kShortName, &value));
  EXPECT_EQ(kName, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kKeyword, &value));
  EXPECT_EQ(kKeyword, value);

  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kSuggestionsURL, &value));
  EXPECT_EQ(kSuggestURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kFaviconURL, &value));
  EXPECT_EQ(kIconURL, value);

  base::ListValue encodings;
  encodings.AppendString("UTF-16");
  encodings.AppendString("UTF-8");

  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kInputEncodings, &list_value));
  EXPECT_TRUE(encodings.Equals(list_value));

  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kAlternateURLs, &list_value));
  EXPECT_TRUE(default_alternate_urls_.Equals(list_value));

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kImageURL, &value));
  EXPECT_EQ(kImageURL, value);

  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kImageURLPostParams, &value));
  EXPECT_EQ(kImageParams, value);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kSearchURLPostParams,
                                    &value));
  EXPECT_EQ(std::string(), value);

  EXPECT_TRUE(dictionary->GetString(
      DefaultSearchManager::kSuggestionsURLPostParams, &value));
  EXPECT_EQ(std::string(), value);
}

// Checks that disabling default search is properly reflected the dictionary
// pref.
TEST_F(DefaultSearchPolicyHandlerTest, DisabledByPolicy) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(false), nullptr);
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://a/?{searchTerms}"), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* temp = nullptr;
  // Ignore any other search provider related policy in this case.
  EXPECT_FALSE(store_->GetValue(DefaultSearchManager::kURL, &temp));

  const base::DictionaryValue* dictionary;
  EXPECT_TRUE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
  temp->GetAsDictionary(&dictionary);
  bool disabled = false;
  EXPECT_TRUE(dictionary->GetBoolean(DefaultSearchManager::kDisabledByPolicy,
                                     &disabled));
  EXPECT_TRUE(disabled);
}

// Check that when the default search enabled policy is not set, all other
// default search-related policies are ignored.
TEST_F(DefaultSearchPolicyHandlerTest, DisabledByPolicyNotSet) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>("http://a/?{searchTerms}"), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* temp = nullptr;
  EXPECT_FALSE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
  EXPECT_FALSE(store_->GetValue(DefaultSearchManager::kURL, &temp));
}

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(DefaultSearchPolicyHandlerTest, MinimallyDefined) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(true), nullptr);
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kSearchURL), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* temp = nullptr;
  const base::DictionaryValue* dictionary;
  std::string value;
  const base::ListValue* list_value;
  EXPECT_TRUE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
  temp->GetAsDictionary(&dictionary);

  // Name and keyword should be derived from host.
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kURL, &value));
  EXPECT_EQ(kSearchURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kShortName, &value));
  EXPECT_EQ(kHostName, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kKeyword, &value));
  EXPECT_EQ(kHostName, value);

  // Everything else should be set to the default value.
  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kSuggestionsURL, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kFaviconURL, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kInputEncodings, &list_value));
  EXPECT_TRUE(base::ListValue().Equals(list_value));
  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kAlternateURLs, &list_value));
  EXPECT_TRUE(base::ListValue().Equals(list_value));
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kImageURL, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kImageURLPostParams, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kSearchURLPostParams,
                                    &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(
      DefaultSearchManager::kSuggestionsURLPostParams, &value));
  EXPECT_EQ(std::string(), value);
}

// Checks that setting a file URL as the default search is reflected properly in
// the dictionary pref.
TEST_F(DefaultSearchPolicyHandlerTest, FileURL) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(true), nullptr);
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kFileSearchURL), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* temp = nullptr;
  const base::DictionaryValue* dictionary;
  std::string value;

  EXPECT_TRUE(store_->GetValue(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName, &temp));
  temp->GetAsDictionary(&dictionary);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kURL, &value));
  EXPECT_EQ(kFileSearchURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kShortName, &value));
  EXPECT_EQ("_", value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kKeyword, &value));
  EXPECT_EQ("_", value);
}

}  // namespace policy
