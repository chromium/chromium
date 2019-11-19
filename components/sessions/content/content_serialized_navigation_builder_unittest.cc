// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_builder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sessions/content/content_serialized_navigation_driver.h"
#include "components/sessions/content/extended_info_handler.h"
#include "components/sessions/content/navigation_task_id.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

namespace {

const char kExtendedInfoKey1[] = "Key 1";
const char kExtendedInfoValue1[] = "Value 1";
const char kExtendedInfoKey2[] = "Key 2";
const char kExtendedInfoValue2[] = "Value 2";

struct TestData : public base::SupportsUserData::Data {
  explicit TestData(const std::string& string) : string(string) {}
  ~TestData() override = default;

  std::string string;
};

class TestExtendedInfoHandler : public ExtendedInfoHandler {
 public:
  explicit TestExtendedInfoHandler(const char* key) : key_(key) {}
  ~TestExtendedInfoHandler() override {}

  // ExtendedInfoHandler:
  std::string GetExtendedInfo(content::NavigationEntry* entry) const override {
    TestData* test_data = static_cast<TestData*>(entry->GetUserData(key_));
    return test_data ? test_data->string : std::string();
  }

  void RestoreExtendedInfo(const std::string& info,
                           content::NavigationEntry* entry) override {
    entry->SetUserData(key_, std::make_unique<TestData>(info));
  }

 private:
  const char* key_;

  DISALLOW_COPY_AND_ASSIGN(TestExtendedInfoHandler);
};

// Create a NavigationEntry from the test_data constants in
// serialized_navigation_entry_test_helper.h.
std::unique_ptr<content::NavigationEntry> MakeNavigationEntryForTest() {
  std::unique_ptr<content::NavigationEntry> navigation_entry(
      content::NavigationEntry::Create());
  navigation_entry->SetReferrer(content::Referrer(
      test_data::kReferrerURL,
      static_cast<network::mojom::ReferrerPolicy>(test_data::kReferrerPolicy)));
  navigation_entry->SetURL(test_data::kURL);
  navigation_entry->SetVirtualURL(test_data::kVirtualURL);
  navigation_entry->SetTitle(test_data::kTitle);
  navigation_entry->SetTransitionType(test_data::kTransitionType);
  navigation_entry->SetHasPostData(test_data::kHasPostData);
  navigation_entry->SetPostID(test_data::kPostID);
  navigation_entry->SetOriginalRequestURL(test_data::kOriginalRequestURL);
  navigation_entry->SetIsOverridingUserAgent(test_data::kIsOverridingUserAgent);
  navigation_entry->SetTimestamp(test_data::kTimestamp);
  SetPasswordStateInNavigation(test_data::kPasswordState,
                               navigation_entry.get());
  navigation_entry->GetFavicon().valid = true;
  navigation_entry->GetFavicon().url = test_data::kFaviconURL;
  navigation_entry->SetHttpStatusCode(test_data::kHttpStatusCode);
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(test_data::kRedirectURL0);
  redirect_chain.push_back(test_data::kRedirectURL1);
  redirect_chain.push_back(test_data::kVirtualURL);
  navigation_entry->SetRedirectChain(redirect_chain);
  NavigationTaskId::Get(navigation_entry.get())->set_id(test_data::kTaskId);
  NavigationTaskId::Get(navigation_entry.get())
      ->set_parent_id(test_data::kParentTaskId);
  NavigationTaskId::Get(navigation_entry.get())
      ->set_root_id(test_data::kRootTaskId);
  NavigationTaskId::Get(navigation_entry.get())
      ->set_children_ids(test_data::kChildrenTaskIds);
  return navigation_entry;
}

void SetExtendedInfoForTest(content::NavigationEntry* entry) {
  entry->SetUserData(kExtendedInfoKey1,
                     std::make_unique<TestData>(kExtendedInfoValue1));
  entry->SetUserData(kExtendedInfoKey2,
                     std::make_unique<TestData>(kExtendedInfoValue2));
}

}  // namespace

class ContentSerializedNavigationBuilderTest : public testing::Test {
 public:
  ContentSerializedNavigationBuilderTest() {}
  ~ContentSerializedNavigationBuilderTest() override {}

  void SetUp() override {
    ContentSerializedNavigationDriver* driver =
        ContentSerializedNavigationDriver::GetInstance();
    driver->RegisterExtendedInfoHandler(
        kExtendedInfoKey1, base::WrapUnique<ExtendedInfoHandler>(
                               new TestExtendedInfoHandler(kExtendedInfoKey1)));
    driver->RegisterExtendedInfoHandler(
        kExtendedInfoKey2, base::WrapUnique<ExtendedInfoHandler>(
                               new TestExtendedInfoHandler(kExtendedInfoKey2)));
  }

  void TearDown() override {
    ContentSerializedNavigationDriver* driver =
        ContentSerializedNavigationDriver::GetInstance();
    driver->extended_info_handler_map_.clear();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSerializedNavigationBuilderTest);
};

// Create a SerializedNavigationEntry from a NavigationEntry.  All its fields
// should match the NavigationEntry's.
TEST_F(ContentSerializedNavigationBuilderTest, FromNavigationEntry) {
  const std::unique_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());
  SetExtendedInfoForTest(navigation_entry.get());

  const SerializedNavigationEntry& navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, navigation_entry.get());

  EXPECT_EQ(test_data::kIndex, navigation.index());

  EXPECT_EQ(navigation_entry->GetUniqueID(), navigation.unique_id());
  EXPECT_EQ(test_data::kReferrerURL, navigation.referrer_url());
  EXPECT_EQ(test_data::kReferrerPolicy, navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_EQ(navigation_entry->GetPageState().ToEncodedData(),
            navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_EQ(test_data::kHasPostData, navigation.has_post_data());
  EXPECT_EQ(test_data::kPostID, navigation.post_id());
  EXPECT_EQ(test_data::kOriginalRequestURL, navigation.original_request_url());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            navigation.is_overriding_user_agent());
  EXPECT_EQ(test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(test_data::kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(test_data::kHttpStatusCode, navigation.http_status_code());
  ASSERT_EQ(3U, navigation.redirect_chain().size());
  EXPECT_EQ(test_data::kRedirectURL0, navigation.redirect_chain()[0]);
  EXPECT_EQ(test_data::kRedirectURL1, navigation.redirect_chain()[1]);
  EXPECT_EQ(test_data::kVirtualURL, navigation.redirect_chain()[2]);
  EXPECT_EQ(test_data::kPasswordState, navigation.password_state());

  ASSERT_EQ(2U, navigation.extended_info_map().size());
  ASSERT_EQ(1U, navigation.extended_info_map().count(kExtendedInfoKey1));
  EXPECT_EQ(kExtendedInfoValue1,
            navigation.extended_info_map().at(kExtendedInfoKey1));
  ASSERT_EQ(1U, navigation.extended_info_map().count(kExtendedInfoKey2));
  EXPECT_EQ(kExtendedInfoValue2,
            navigation.extended_info_map().at(kExtendedInfoKey2));

  EXPECT_EQ(test_data::kTaskId, navigation.task_id());
  EXPECT_EQ(test_data::kParentTaskId, navigation.parent_task_id());
  EXPECT_EQ(test_data::kRootTaskId, navigation.root_task_id());
  EXPECT_EQ(test_data::kChildrenTaskIds, navigation.children_task_ids());
}

// Test effect of the navigation serialization options.
TEST_F(ContentSerializedNavigationBuilderTest,
       FromNavigationEntrySerializationOptions) {
  const std::unique_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& default_navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, navigation_entry.get(),
          ContentSerializedNavigationBuilder::DEFAULT);
  EXPECT_EQ(navigation_entry->GetPageState().ToEncodedData(),
            default_navigation.encoded_page_state());

  const SerializedNavigationEntry& excluded_page_state_navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, navigation_entry.get(),
          ContentSerializedNavigationBuilder::EXCLUDE_PAGE_STATE);
  EXPECT_TRUE(excluded_page_state_navigation.encoded_page_state().empty());
}

// Create a NavigationEntry, then create another one by converting to
// a SerializedNavigationEntry and back.  The new one should match the old one
// except for fields that aren't preserved, which should be set to
// expected values.
TEST_F(ContentSerializedNavigationBuilderTest, ToNavigationEntry) {
  const std::unique_ptr<content::NavigationEntry> old_navigation_entry(
      MakeNavigationEntryForTest());
  SetExtendedInfoForTest(old_navigation_entry.get());

  const SerializedNavigationEntry& navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, old_navigation_entry.get());

  const std::unique_ptr<content::NavigationEntry> new_navigation_entry(
      ContentSerializedNavigationBuilder::ToNavigationEntry(&navigation,
                                                            nullptr));

  EXPECT_EQ(test_data::kReferrerURL, new_navigation_entry->GetReferrer().url);
  EXPECT_EQ(test_data::kReferrerPolicy,
            static_cast<int>(new_navigation_entry->GetReferrer().policy));
  EXPECT_EQ(test_data::kURL, new_navigation_entry->GetURL());
  EXPECT_EQ(test_data::kVirtualURL, new_navigation_entry->GetVirtualURL());
  EXPECT_EQ(test_data::kTitle, new_navigation_entry->GetTitle());
  EXPECT_EQ(old_navigation_entry->GetPageState().ToEncodedData(),
            new_navigation_entry->GetPageState().ToEncodedData());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      new_navigation_entry->GetTransitionType(), ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(test_data::kHasPostData, new_navigation_entry->GetHasPostData());
  EXPECT_EQ(test_data::kPostID, new_navigation_entry->GetPostID());
  EXPECT_EQ(test_data::kOriginalRequestURL,
            new_navigation_entry->GetOriginalRequestURL());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            new_navigation_entry->GetIsOverridingUserAgent());
  EXPECT_EQ(test_data::kHttpStatusCode,
            new_navigation_entry->GetHttpStatusCode());
  ASSERT_EQ(3U, new_navigation_entry->GetRedirectChain().size());
  EXPECT_EQ(test_data::kRedirectURL0,
            new_navigation_entry->GetRedirectChain()[0]);
  EXPECT_EQ(test_data::kRedirectURL1,
            new_navigation_entry->GetRedirectChain()[1]);
  EXPECT_EQ(test_data::kVirtualURL,
            new_navigation_entry->GetRedirectChain()[2]);
  sessions::NavigationTaskId* new_navigation_task_id =
      sessions::NavigationTaskId::Get(new_navigation_entry.get());

  EXPECT_EQ(test_data::kTaskId, new_navigation_task_id->id());
  EXPECT_EQ(test_data::kParentTaskId, new_navigation_task_id->parent_id());
  EXPECT_EQ(test_data::kRootTaskId, new_navigation_task_id->root_id());

  TestData* test_data = static_cast<TestData*>(
      new_navigation_entry->GetUserData(kExtendedInfoKey1));
  ASSERT_TRUE(test_data);
  EXPECT_EQ(kExtendedInfoValue1, test_data->string);
  test_data = static_cast<TestData*>(
      new_navigation_entry->GetUserData(kExtendedInfoKey2));
  ASSERT_TRUE(test_data);
  EXPECT_EQ(kExtendedInfoValue2, test_data->string);
}

TEST_F(ContentSerializedNavigationBuilderTest, SetPasswordState) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());

  EXPECT_EQ(SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
            GetPasswordStateFromNavigation(entry.get()));
  SetPasswordStateInNavigation(SerializedNavigationEntry::NO_PASSWORD_FIELD,
                               entry.get());
  EXPECT_EQ(SerializedNavigationEntry::NO_PASSWORD_FIELD,
            GetPasswordStateFromNavigation(entry.get()));
}

}  // namespace sessions
