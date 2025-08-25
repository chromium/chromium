// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/enterprise_shortcuts_store.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

const char16_t kTestTitle1[] = u"Foo1";
const char16_t kTestTitle2[] = u"Foo2";
const char kTestUrl1[] = "http://foo1.com/";
const char kTestUrl2[] = "http://foo2.com/";

base::Value::Dict CreatePolicyLink(const std::string& url,
                                   const std::string& title,
                                   bool allow_user_edit,
                                   bool allow_user_delete) {
  base::Value::Dict link;
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyUrl, url);
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyTitle, title);
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyPolicyOrigin,
           static_cast<int>(EnterpriseShortcut::PolicyOrigin::kNtpShortcuts));
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyIsHiddenByUser, false);
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete,
           allow_user_delete);
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserEdit,
           allow_user_edit);
  link.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete,
           allow_user_delete);
  return link;
}

EnterpriseShortcut CreateEnterpriseShortcut(
    const GURL& url,
    const std::u16string& title,
    EnterpriseShortcut::PolicyOrigin policy_origin,
    bool is_hidden_by_user,
    bool allow_user_edit,
    bool allow_user_delete) {
  EnterpriseShortcut link;
  link.url = url;
  link.title = title;
  link.policy_origin = policy_origin;
  link.is_hidden_by_user = is_hidden_by_user;
  link.allow_user_edit = allow_user_edit;
  link.allow_user_delete = allow_user_delete;
  return link;
}

}  // namespace

class EnterpriseShortcutsStoreTest : public testing::Test {
 public:
  EnterpriseShortcutsStoreTest() {
    enterprise_shortcuts_store_ =
        std::make_unique<EnterpriseShortcutsStore>(&prefs_);
    EnterpriseShortcutsStore::RegisterProfilePrefs(prefs_.registry());
  }

  EnterpriseShortcutsStoreTest(const EnterpriseShortcutsStoreTest&) = delete;
  EnterpriseShortcutsStoreTest& operator=(const EnterpriseShortcutsStoreTest&) =
      delete;

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<EnterpriseShortcutsStore> enterprise_shortcuts_store_;
};

TEST_F(EnterpriseShortcutsStoreTest, StoreAndRetrieveLinks) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  EnterpriseShortcut user_link1 = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  EnterpriseShortcut user_link2 = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  std::vector<EnterpriseShortcut> initial_links({user_link1, user_link2});

  enterprise_shortcuts_store_->StoreLinks(initial_links);
  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  EXPECT_EQ(initial_links, retrieved_links);
}

TEST_F(EnterpriseShortcutsStoreTest, StoreEmptyList) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  EnterpriseShortcut user_link1 = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  EnterpriseShortcut user_link2 = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  std::vector<EnterpriseShortcut> populated_links({user_link1, user_link2});

  enterprise_shortcuts_store_->StoreLinks(populated_links);
  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(populated_links, retrieved_links);

  enterprise_shortcuts_store_->StoreLinks(std::vector<EnterpriseShortcut>());
  retrieved_links = enterprise_shortcuts_store_->RetrieveLinks();
  EXPECT_TRUE(retrieved_links.empty());
}

TEST_F(EnterpriseShortcutsStoreTest, ClearLinks) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  EnterpriseShortcut user_link1 = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  EnterpriseShortcut user_link2 = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  std::vector<EnterpriseShortcut> initial_links({user_link1, user_link2});

  enterprise_shortcuts_store_->StoreLinks(initial_links);
  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(initial_links, retrieved_links);

  enterprise_shortcuts_store_->ClearLinks();
  retrieved_links = enterprise_shortcuts_store_->RetrieveLinks();
  EXPECT_TRUE(retrieved_links.empty());
}

TEST_F(EnterpriseShortcutsStoreTest, LinksSavedAfterShutdown) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  EnterpriseShortcut user_link1 = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  EnterpriseShortcut user_link2 = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  std::vector<EnterpriseShortcut> initial_links({user_link1, user_link2});

  enterprise_shortcuts_store_->StoreLinks(initial_links);
  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(initial_links, retrieved_links);

  // Simulate shutdown by recreating CustomLinksStore.
  enterprise_shortcuts_store_.reset();
  enterprise_shortcuts_store_ =
      std::make_unique<EnterpriseShortcutsStore>(&prefs_);
  retrieved_links = enterprise_shortcuts_store_->RetrieveLinks();
  EXPECT_EQ(initial_links, retrieved_links);
}

TEST_F(EnterpriseShortcutsStoreTest,
       RetrieveLinks_UserLinksEmpty_FallsBackToPolicyLinks) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle1),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/false));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(1u, retrieved_links.size());
  EXPECT_EQ(GURL(kTestUrl1), retrieved_links[0].url);
  EXPECT_EQ(kTestTitle1, retrieved_links[0].title);
  EXPECT_EQ(EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
            retrieved_links[0].policy_origin);
  EXPECT_FALSE(retrieved_links[0].is_hidden_by_user);
  EXPECT_TRUE(retrieved_links[0].allow_user_edit);
  EXPECT_FALSE(retrieved_links[0].allow_user_delete);
}

TEST_F(EnterpriseShortcutsStoreTest,
       RetrieveLinks_UserLinksPresent_ReturnsUserLinks) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle1),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/false));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  EnterpriseShortcut user_link = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2, EnterpriseShortcut::PolicyOrigin::kNoPolicy,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({user_link});

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  EXPECT_EQ(std::vector<EnterpriseShortcut>{user_link}, retrieved_links);
}

TEST_F(EnterpriseShortcutsStoreTest, RetrieveLinks_InvalidDataReturnsEmpty) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  base::Value::List stored_links;
  base::Value::Dict invalid_link;
  invalid_link.Set(EnterpriseShortcutsStore::kDictionaryKeyUrl, kTestUrl1);
  // Missing title.
  stored_links.Append(std::move(invalid_link));
  prefs_.SetList(prefs::kEnterpriseShortcutsUserList, std::move(stored_links));

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  EXPECT_TRUE(retrieved_links.empty());
  EXPECT_FALSE(prefs_.GetList(prefs::kEnterpriseShortcutsUserList).empty());
}

TEST_F(EnterpriseShortcutsStoreTest,
       OnPreferenceChanged_NoUserLinks_PolicySet) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  ASSERT_TRUE(enterprise_shortcuts_store_->RetrieveLinks().empty());

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle1),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/false));
  policy_links.Append(
      CreatePolicyLink(kTestUrl2, base::UTF16ToUTF8(kTestTitle2),
                       /*allow_user_edit=*/false, /*allow_user_delete=*/true));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(2u, retrieved_links.size());

  EnterpriseShortcut expected_link1 = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false, /*allow_user_edit=*/true,
      /*allow_user_delete=*/false);
  EnterpriseShortcut expected_link2 = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false, /*allow_user_edit=*/false,
      /*allow_user_delete=*/true);

  EXPECT_EQ(expected_link1, retrieved_links[0]);
  EXPECT_EQ(expected_link2, retrieved_links[1]);
}

TEST_F(EnterpriseShortcutsStoreTest, OnPreferenceChanged_PolicyRemoved) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  EnterpriseShortcut link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({link});
  ASSERT_FALSE(enterprise_shortcuts_store_->RetrieveLinks().empty());

  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList, base::Value::List());

  EXPECT_TRUE(enterprise_shortcuts_store_->RetrieveLinks().empty());
}

TEST_F(EnterpriseShortcutsStoreTest, OnPreferenceChanged_Merge_AddNewLink) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  EnterpriseShortcut user_link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({user_link});

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(1u, retrieved_links.size());

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle1),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/true));
  policy_links.Append(
      CreatePolicyLink(kTestUrl2, base::UTF16ToUTF8(kTestTitle2),
                       /*allow_user_edit=*/false, /*allow_user_delete=*/false));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  retrieved_links = enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(2u, retrieved_links.size());
  EXPECT_EQ(user_link, retrieved_links[0]);

  EnterpriseShortcut expected_new_link = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false, /*allow_user_edit=*/false,
      /*allow_user_delete=*/false);
  EXPECT_EQ(expected_new_link, retrieved_links[1]);
}

TEST_F(EnterpriseShortcutsStoreTest, OnPreferenceChanged_Merge_RemoveLink) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  EnterpriseShortcut user_link1 = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  EnterpriseShortcut user_link2 = CreateEnterpriseShortcut(
      GURL(kTestUrl2), kTestTitle2,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({user_link1, user_link2});

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(2u, retrieved_links.size());

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle1),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/true));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  retrieved_links = enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(1u, retrieved_links.size());
  EXPECT_EQ(user_link1, retrieved_links[0]);
}

TEST_F(EnterpriseShortcutsStoreTest, OnPreferenceChanged_Merge_UpdateLink) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  EnterpriseShortcut user_link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({user_link});

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle2),
                       /*allow_user_edit=*/false, /*allow_user_delete=*/false));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(1u, retrieved_links.size());
  EXPECT_EQ(GURL(kTestUrl1), retrieved_links[0].url);
  EXPECT_EQ(kTestTitle2, retrieved_links[0].title);
  EXPECT_FALSE(retrieved_links[0].allow_user_edit);
  EXPECT_FALSE(retrieved_links[0].allow_user_delete);
}

TEST_F(EnterpriseShortcutsStoreTest,
       OnPreferenceChanged_Merge_UpdateUserModifiedLink) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  EnterpriseShortcut user_link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1, EnterpriseShortcut::PolicyOrigin::kNoPolicy,
      /*is_hidden_by_user=*/false,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({user_link});

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle2),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/false));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(1u, retrieved_links.size());

  // Since the link has been modified by the user, `allow_user_edit` and
  // `allow_user_delete should be updated.
  EnterpriseShortcut expected_link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1, EnterpriseShortcut::PolicyOrigin::kNoPolicy,
      /*is_hidden_by_user=*/false, /*allow_user_edit=*/true,
      /*allow_user_delete=*/false);
  EXPECT_EQ(expected_link, retrieved_links[0]);
}

TEST_F(EnterpriseShortcutsStoreTest,
       OnPreferenceChanged_Merge_UpdateUserHiddenLink) {
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_store_->RegisterCallbackForOnChanged(callback.Get());
  EXPECT_CALL(callback, Run());

  EnterpriseShortcut user_link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/true,
      /*allow_user_edit=*/true, /*allow_user_delete=*/true);
  enterprise_shortcuts_store_->StoreLinks({user_link});

  base::Value::List policy_links;
  policy_links.Append(
      CreatePolicyLink(kTestUrl1, base::UTF16ToUTF8(kTestTitle1),
                       /*allow_user_edit=*/true, /*allow_user_delete=*/false));
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList,
                 std::move(policy_links));

  std::vector<EnterpriseShortcut> retrieved_links =
      enterprise_shortcuts_store_->RetrieveLinks();
  ASSERT_EQ(1u, retrieved_links.size());

  // Since `allow_user_delete` is false, the link should no longer be hidden.
  EnterpriseShortcut expected_link = CreateEnterpriseShortcut(
      GURL(kTestUrl1), kTestTitle1,
      EnterpriseShortcut::PolicyOrigin::kNtpShortcuts,
      /*is_hidden_by_user=*/false, /*allow_user_edit=*/true,
      /*allow_user_delete=*/false);
  EXPECT_EQ(expected_link, retrieved_links[0]);
}

}  // namespace ntp_tiles
