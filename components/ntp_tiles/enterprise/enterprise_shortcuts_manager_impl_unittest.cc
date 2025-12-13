// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager_impl.h"

#include <stdint.h>

#include <array>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcut.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_store.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_tiles {

namespace {

struct TestCaseItem {
  const char* url;
  const char16_t* title;
};

const auto kOneLinkTestCase =
    std::to_array<TestCaseItem>({{"http://foo1.com/", u"Foo1"}});
constexpr auto kTwoLinksTestCase = std::to_array<TestCaseItem>({
    {"http://foo1.com/", u"Foo1"},
    {"http://foo2.com/", u"Foo2"},
});
constexpr auto kThreeLinksTestCase = std::to_array<TestCaseItem>({
    {"http://foo1.com/", u"Foo1"},
    {"http://foo2.com/", u"Foo2"},
    {"http://foo3.com/", u"Foo3"},
});

const char16_t kTestTitle16[] = u"Test";
const char kTestUrl[] = "http://test.com/";

void SetPolicyLinks(PrefService* prefs,
                    base::span<const TestCaseItem> test_cases,
                    bool allow_user_edit = false,
                    bool allow_user_delete = false) {
  base::Value::List links;
  for (const auto& test_case : test_cases) {
    base::Value::Dict link;
    link.Set(EnterpriseShortcutsStore::kDictionaryKeyUrl, test_case.url);
    link.Set(EnterpriseShortcutsStore::kDictionaryKeyTitle,
             base::UTF16ToUTF8(test_case.title));
    link.Set(EnterpriseShortcutsStore::kDictionaryKeyPolicyOrigin,
             static_cast<int>(EnterpriseShortcut::PolicyOrigin::kNtpShortcuts));
    link.Set(EnterpriseShortcutsStore::kDictionaryKeyIsHiddenByUser, false);
    link.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserEdit,
             allow_user_edit);
    link.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete,
             allow_user_delete);
    links.Append(std::move(link));
  }
  prefs->SetList(prefs::kEnterpriseShortcutsPolicyList, std::move(links));
}

std::vector<EnterpriseShortcut> FillTestLinks(
    base::span<const TestCaseItem> test_cases,
    bool allow_user_edit = false,
    bool allow_user_delete = false) {
  std::vector<EnterpriseShortcut> links;
  for (const auto& test_case : test_cases) {
    EnterpriseShortcut link;
    link.url = GURL(test_case.url);
    link.title = test_case.title;
    link.policy_origin = EnterpriseShortcut::PolicyOrigin::kNtpShortcuts;
    link.is_hidden_by_user = false;
    link.allow_user_edit = allow_user_edit;
    link.allow_user_delete = allow_user_delete;
    links.emplace_back(link);
  }
  return links;
}

}  // namespace

class EnterpriseShortcutsManagerImplTest : public testing::Test {
 public:
  EnterpriseShortcutsManagerImplTest() {
    EnterpriseShortcutsManagerImpl::RegisterProfilePrefs(prefs_.registry());
  }

  EnterpriseShortcutsManagerImplTest(
      const EnterpriseShortcutsManagerImplTest&) = delete;
  EnterpriseShortcutsManagerImplTest& operator=(
      const EnterpriseShortcutsManagerImplTest&) = delete;

  void SetUp() override {
    enterprise_shortcuts_ =
        std::make_unique<EnterpriseShortcutsManagerImpl>(&prefs_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<EnterpriseShortcutsManagerImpl> enterprise_shortcuts_;
};

TEST_F(EnterpriseShortcutsManagerImplTest, LoadsPolicyLinksOnStartup) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase);

  // Simulate shutdown by recreating EnterpriseShortcutsManagerImpl.
  enterprise_shortcuts_ =
      std::make_unique<EnterpriseShortcutsManagerImpl>(&prefs_);
  EXPECT_EQ(FillTestLinks(kOneLinkTestCase), enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest, LoadsUserModifiedLinksOnStartup) {
  // Set policy links.
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/true);
  enterprise_shortcuts_ =
      std::make_unique<EnterpriseShortcutsManagerImpl>(&prefs_);
  ASSERT_EQ(FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true),
            enterprise_shortcuts_->GetLinks());

  // Create some user links by updating a link.
  enterprise_shortcuts_->UpdateLink(GURL(kOneLinkTestCase[0].url),
                                    u"new title");
  std::vector<EnterpriseShortcut> expected_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true);
  expected_links[0].title = u"new title";
  expected_links[0].policy_origin = EnterpriseShortcut::PolicyOrigin::kNoPolicy;
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());

  // Simulate shutdown by recreating EnterpriseShortcutsManagerImpl.
  enterprise_shortcuts_ =
      std::make_unique<EnterpriseShortcutsManagerImpl>(&prefs_);
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       RestorePolicyLinks_ClearsUserLinksAndResetsToPolicy) {
  // Set policy links.
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/true);
  enterprise_shortcuts_ =
      std::make_unique<EnterpriseShortcutsManagerImpl>(&prefs_);
  ASSERT_EQ(FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true),
            enterprise_shortcuts_->GetLinks());

  // Create some user links by updating a link.
  enterprise_shortcuts_->UpdateLink(GURL(kOneLinkTestCase[0].url),
                                    u"new title");
  std::vector<EnterpriseShortcut> expected_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true);
  expected_links[0].title = u"new title";
  expected_links[0].policy_origin = EnterpriseShortcut::PolicyOrigin::kNoPolicy;
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());

  // RestorePolicyLinks should clear user links and fall back to policy links.
  enterprise_shortcuts_->RestorePolicyLinks();
  EXPECT_EQ(FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true),
            enterprise_shortcuts_->GetLinks());

  // RestorePolicyLinks with no policy links. Should do nothing.
  SetPolicyLinks(&prefs_, {});
  enterprise_shortcuts_->RestorePolicyLinks();
  EXPECT_TRUE(enterprise_shortcuts_->GetLinks().empty());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       UpdateLink_WhenAllowedByPolicy_Succeeds) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/true);
  ASSERT_EQ(FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true),
            enterprise_shortcuts_->GetLinks());

  // Update the EnterpriseShortcut's title.
  EXPECT_TRUE(enterprise_shortcuts_->UpdateLink(GURL(kOneLinkTestCase[0].url),
                                                kTestTitle16));
  std::vector<EnterpriseShortcut> expected_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true);
  expected_links[0].title = kTestTitle16;
  expected_links[0].policy_origin = EnterpriseShortcut::PolicyOrigin::kNoPolicy;
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       UpdateLink_WhenDisallowedByPolicy_Fails) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/false);
  std::vector<EnterpriseShortcut> initial_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/false);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Update the EnterpriseShortcut's title. This should fail.
  EXPECT_FALSE(enterprise_shortcuts_->UpdateLink(GURL(kOneLinkTestCase[0].url),
                                                 kTestTitle16));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       UpdateLink_WithInvalidParameters_Fails) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase);
  std::vector<EnterpriseShortcut> initial_links =
      FillTestLinks(kOneLinkTestCase);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to update a EnterpriseShortcut that does not exist. This should fail
  // and not modify the list.
  EXPECT_FALSE(enterprise_shortcuts_->UpdateLink(GURL(kTestUrl), kTestTitle16));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to pass empty title. This should fail and not modify the list.
  EXPECT_FALSE(enterprise_shortcuts_->UpdateLink(GURL(kOneLinkTestCase[0].url),
                                                 std::u16string()));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to pass an invalid URL. This should fail and not modify the list.
  EXPECT_FALSE(enterprise_shortcuts_->UpdateLink(GURL("test"), kTestTitle16));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       ReorderLink_WithVariousPositions_Succeeds) {
  SetPolicyLinks(&prefs_, kThreeLinksTestCase);
  std::vector<EnterpriseShortcut> initial_links =
      FillTestLinks(kThreeLinksTestCase);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to call reorder with the current index. This should fail and not modify
  // the list.
  EXPECT_FALSE(enterprise_shortcuts_->ReorderLink(
      GURL(kThreeLinksTestCase[2].url), (size_t)2));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to call reorder with an invalid index. This should fail and not modify
  // the list.
  EXPECT_FALSE(enterprise_shortcuts_->ReorderLink(
      GURL(kThreeLinksTestCase[2].url), (size_t)-1));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
  EXPECT_FALSE(enterprise_shortcuts_->ReorderLink(
      GURL(kThreeLinksTestCase[2].url), initial_links.size()));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to call reorder with an invalid URL. This should fail and not modify
  // the list.
  EXPECT_FALSE(enterprise_shortcuts_->ReorderLink(GURL(kTestUrl), 0));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
  EXPECT_FALSE(enterprise_shortcuts_->ReorderLink(GURL("test"), 0));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Move the last EnterpriseShortcut to the front.
  EXPECT_TRUE(enterprise_shortcuts_->ReorderLink(
      GURL(kThreeLinksTestCase[2].url), (size_t)0));
  std::vector<EnterpriseShortcut> expected_links;
  expected_links.push_back(initial_links[2]);
  expected_links.push_back(initial_links[0]);
  expected_links.push_back(initial_links[1]);
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());

  // Move the same EnterpriseShortcut to the right.
  EXPECT_TRUE(enterprise_shortcuts_->ReorderLink(
      GURL(kThreeLinksTestCase[2].url), (size_t)1));
  expected_links.clear();
  expected_links.push_back(initial_links[0]);
  expected_links.push_back(initial_links[2]);
  expected_links.push_back(initial_links[1]);
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());

  // Move the same EnterpriseShortcut to the end.
  EXPECT_TRUE(enterprise_shortcuts_->ReorderLink(
      GURL(kThreeLinksTestCase[2].url), (size_t)2));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       DeleteLink_WhenAllowedByPolicy_Succeeds) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/false,
                 /*allow_user_delete=*/true);
  ASSERT_EQ(FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/false,
                          /*allow_user_delete=*/true),
            enterprise_shortcuts_->GetLinks());

  // Delete EnterpriseShortcut.
  EXPECT_TRUE(enterprise_shortcuts_->DeleteLink(GURL(kOneLinkTestCase[0].url)));
  std::vector<EnterpriseShortcut> expected_links = FillTestLinks(
      kOneLinkTestCase, /*allow_user_edit=*/false, /*allow_user_delete=*/true);
  expected_links[0].is_hidden_by_user = true;
  EXPECT_EQ(expected_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       DeleteLink_WhenDisallowedByPolicy_Fails) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/false,
                 /*allow_user_delete=*/false);
  std::vector<EnterpriseShortcut> initial_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/false,
                    /*allow_user_delete=*/false);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Delete EnterpriseShortcut. This should fail.
  EXPECT_FALSE(
      enterprise_shortcuts_->DeleteLink(GURL(kOneLinkTestCase[0].url)));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       DeleteLink_WithNonExistentUrl_Fails) {
  ASSERT_TRUE(enterprise_shortcuts_->GetLinks().empty());

  // Try to delete EnterpriseShortcut. This should fail and not modify the list.
  EXPECT_FALSE(enterprise_shortcuts_->DeleteLink(GURL(kTestUrl)));
  EXPECT_TRUE(enterprise_shortcuts_->GetLinks().empty());
}

TEST_F(EnterpriseShortcutsManagerImplTest, DeleteLink_WithInvalidUrl_Fails) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_edit=*/false,
                 /*allow_delete=*/true);
  std::vector<EnterpriseShortcut> initial_links = FillTestLinks(
      kOneLinkTestCase, /*allow_edit=*/false, /*allow_delete=*/true);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to delete EnterpriseShortcut with an invalid URL. This should fail.
  EXPECT_FALSE(enterprise_shortcuts_->DeleteLink(GURL("invalid-url")));
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       Undo_AfterUpdate_RestoresPreviousState) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/true);
  std::vector<EnterpriseShortcut> initial_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Update the EnterpriseShortcut's title.
  EXPECT_TRUE(enterprise_shortcuts_->UpdateLink(GURL(kOneLinkTestCase[0].url),
                                                kTestTitle16));
  std::vector<EnterpriseShortcut> updated_links =
      FillTestLinks(kOneLinkTestCase, /*allow_user_edit=*/true);
  updated_links[0].title = kTestTitle16;
  updated_links[0].policy_origin = EnterpriseShortcut::PolicyOrigin::kNoPolicy;
  EXPECT_EQ(updated_links, enterprise_shortcuts_->GetLinks());

  // Undo update EnterpriseShortcut.
  EXPECT_TRUE(enterprise_shortcuts_->UndoAction());
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Try to undo again. This should fail and not modify the list.
  EXPECT_FALSE(enterprise_shortcuts_->UndoAction());
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       Undo_AfterDelete_RestoresPreviousState) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase, /*allow_user_edit=*/false,
                 /*allow_user_delete=*/true);
  std::vector<EnterpriseShortcut> initial_links = FillTestLinks(
      kOneLinkTestCase, /*allow_user_edit=*/false, /*allow_user_delete=*/true);
  ASSERT_EQ(initial_links, enterprise_shortcuts_->GetLinks());

  // Delete EnterpriseShortcut.
  ASSERT_TRUE(enterprise_shortcuts_->DeleteLink(GURL(kOneLinkTestCase[0].url)));
  std::vector<EnterpriseShortcut> deleted_links = FillTestLinks(
      kOneLinkTestCase, /*allow_user_edit=*/false, /*allow_user_delete=*/true);
  deleted_links[0].is_hidden_by_user = true;
  ASSERT_EQ(deleted_links, enterprise_shortcuts_->GetLinks());

  // Undo delete EnterpriseShortcut.
  EXPECT_TRUE(enterprise_shortcuts_->UndoAction());
  EXPECT_EQ(initial_links, enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       OnPolicyChanged_NotifiesObserverAndUpdateLinks) {
  // Set up callback.
  base::MockCallback<base::RepeatingClosure> callback;
  base::CallbackListSubscription subscription =
      enterprise_shortcuts_->RegisterCallbackForOnChanged(callback.Get());

  ASSERT_TRUE(enterprise_shortcuts_->GetLinks().empty());

  // Set policy for the first time.
  EXPECT_CALL(callback, Run());
  SetPolicyLinks(&prefs_, kOneLinkTestCase);
  ASSERT_EQ(FillTestLinks(kOneLinkTestCase), enterprise_shortcuts_->GetLinks());

  // Modify the policy. This should notify and update the current list of links.
  EXPECT_CALL(callback, Run());
  SetPolicyLinks(&prefs_, kTwoLinksTestCase);
  EXPECT_EQ(FillTestLinks(kTwoLinksTestCase),
            enterprise_shortcuts_->GetLinks());
}

TEST_F(EnterpriseShortcutsManagerImplTest,
       OnPolicyRemoved_NotifiesObserverAndClearsLinks) {
  SetPolicyLinks(&prefs_, kOneLinkTestCase);
  ASSERT_EQ(FillTestLinks(kOneLinkTestCase), enterprise_shortcuts_->GetLinks());

  // Remove the policy. This should notify and clear custom links.
  prefs_.SetList(prefs::kEnterpriseShortcutsPolicyList, base::Value::List());
  EXPECT_TRUE(enterprise_shortcuts_->GetLinks().empty());
}

}  // namespace ntp_tiles
