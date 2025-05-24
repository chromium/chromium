// Copyright 2014 The Chromium Authors
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
#include "content/public/browser/navigation_entry_restore_context.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state.h"

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

  TestExtendedInfoHandler(const TestExtendedInfoHandler&) = delete;
  TestExtendedInfoHandler& operator=(const TestExtendedInfoHandler&) = delete;

  ~TestExtendedInfoHandler() override = default;

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
};

std::unique_ptr<content::NavigationEntry> MakeNavigationEntryForTest() {
  std::unique_ptr<content::NavigationEntry> navigation_entry(
      content::NavigationEntry::Create());
  navigation_entry->SetReferrer(content::Referrer(
      GURL("http://www.referrer.com"),
      static_cast<network::mojom::ReferrerPolicy>(test_data::kReferrerPolicy)));
  navigation_entry->SetURL(GURL("http://www.url.com"));
  navigation_entry->SetVirtualURL(GURL("http://www.virtual-url.com"));
  navigation_entry->SetTitle(test_data::kTitle);
  navigation_entry->SetTransitionType(test_data::kTransitionType);
  navigation_entry->SetHasPostData(test_data::kHasPostData);
  navigation_entry->SetPostID(test_data::kPostID);
  navigation_entry->SetOriginalRequestURL(
      GURL("http://www.original-request.com"));
  navigation_entry->SetIsOverridingUserAgent(test_data::kIsOverridingUserAgent);
  navigation_entry->SetTimestamp(test_data::kTimestamp);
  SetPasswordStateInNavigation(test_data::kPasswordState,
                               navigation_entry.get());
  navigation_entry->GetFavicon().valid = true;
  navigation_entry->GetFavicon().url =
      GURL("http://virtual-url.com/favicon.ico");
  navigation_entry->SetHttpStatusCode(test_data::kHttpStatusCode);
  std::vector<GURL> redirect_chain;
  redirect_chain.emplace_back("http://go/redirect0");
  redirect_chain.emplace_back("http://go/redirect1");
  redirect_chain.push_back(navigation_entry->GetVirtualURL());
  navigation_entry->SetRedirectChain(redirect_chain);
  NavigationTaskId::Get(navigation_entry.get())->set_id(test_data::kTaskId);
  NavigationTaskId::Get(navigation_entry.get())
      ->set_parent_id(test_data::kParentTaskId);
  NavigationTaskId::Get(navigation_entry.get())
      ->set_root_id(test_data::kRootTaskId);
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
  ContentSerializedNavigationBuilderTest() = default;

  ContentSerializedNavigationBuilderTest(
      const ContentSerializedNavigationBuilderTest&) = delete;
  ContentSerializedNavigationBuilderTest& operator=(
      const ContentSerializedNavigationBuilderTest&) = delete;

  ~ContentSerializedNavigationBuilderTest() override = default;

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
  EXPECT_EQ(navigation_entry->GetReferrer().url, navigation.referrer_url());
  EXPECT_EQ(navigation_entry->GetReferrer().policy,
            static_cast<network::mojom::ReferrerPolicy>(
                navigation.referrer_policy()));
  EXPECT_EQ(navigation_entry->GetVirtualURL(), navigation.virtual_url());
  EXPECT_EQ(navigation_entry->GetTitle(), navigation.title());
  EXPECT_EQ(navigation_entry->GetPageState().ToEncodedData(),
            navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), navigation_entry->GetTransitionType()));
  EXPECT_EQ(navigation_entry->GetHasPostData(), navigation.has_post_data());
  EXPECT_EQ(navigation_entry->GetPostID(), navigation.post_id());
  EXPECT_EQ(navigation_entry->GetOriginalRequestURL(),
            navigation.original_request_url());
  EXPECT_EQ(navigation_entry->GetIsOverridingUserAgent(),
            navigation.is_overriding_user_agent());
  EXPECT_EQ(navigation_entry->GetTimestamp(), navigation.timestamp());
  EXPECT_EQ(navigation_entry->GetFavicon().url, navigation.favicon_url());
  EXPECT_EQ(navigation_entry->GetHttpStatusCode(),
            navigation.http_status_code());
  EXPECT_EQ(navigation_entry->GetRedirectChain(), navigation.redirect_chain());
  EXPECT_EQ(GetPasswordStateFromNavigation(navigation_entry.get()),
            navigation.password_state());

  ASSERT_EQ(2U, navigation.extended_info_map().size());
  ASSERT_EQ(1U, navigation.extended_info_map().count(kExtendedInfoKey1));
  EXPECT_EQ(kExtendedInfoValue1,
            navigation.extended_info_map().at(kExtendedInfoKey1));
  ASSERT_EQ(1U, navigation.extended_info_map().count(kExtendedInfoKey2));
  EXPECT_EQ(kExtendedInfoValue2,
            navigation.extended_info_map().at(kExtendedInfoKey2));

  sessions::NavigationTaskId* navigation_task_id =
      sessions::NavigationTaskId::Get(navigation_entry.get());
  EXPECT_EQ(navigation_task_id->id(), navigation.task_id());
  EXPECT_EQ(navigation_task_id->parent_id(), navigation.parent_task_id());
  EXPECT_EQ(navigation_task_id->root_id(), navigation.root_task_id());
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
  content::BrowserTaskEnvironment test_environment;
  content::TestBrowserContext browser_context;

  const std::unique_ptr<content::NavigationEntry> old_navigation_entry(
      MakeNavigationEntryForTest());
  SetExtendedInfoForTest(old_navigation_entry.get());

  const SerializedNavigationEntry& navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, old_navigation_entry.get());

  std::unique_ptr<content::NavigationEntryRestoreContext> restore_context =
      content::NavigationEntryRestoreContext::Create();
  const std::unique_ptr<content::NavigationEntry> new_navigation_entry(
      ContentSerializedNavigationBuilder::ToNavigationEntry(
          &navigation, &browser_context, restore_context.get()));

  EXPECT_EQ(old_navigation_entry->GetReferrer().url,
            new_navigation_entry->GetReferrer().url);
  EXPECT_EQ(old_navigation_entry->GetReferrer().policy,
            new_navigation_entry->GetReferrer().policy);
  EXPECT_EQ(old_navigation_entry->GetURL(), new_navigation_entry->GetURL());
  EXPECT_EQ(old_navigation_entry->GetVirtualURL(),
            new_navigation_entry->GetVirtualURL());
  EXPECT_EQ(old_navigation_entry->GetTitle(), new_navigation_entry->GetTitle());
  EXPECT_EQ(old_navigation_entry->GetPageState().ToEncodedData(),
            new_navigation_entry->GetPageState().ToEncodedData());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      new_navigation_entry->GetTransitionType(), ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(old_navigation_entry->GetHasPostData(),
            new_navigation_entry->GetHasPostData());
  EXPECT_EQ(old_navigation_entry->GetPostID(),
            new_navigation_entry->GetPostID());
  EXPECT_EQ(old_navigation_entry->GetOriginalRequestURL(),
            new_navigation_entry->GetOriginalRequestURL());
  EXPECT_EQ(old_navigation_entry->GetIsOverridingUserAgent(),
            new_navigation_entry->GetIsOverridingUserAgent());
  EXPECT_EQ(old_navigation_entry->GetHttpStatusCode(),
            new_navigation_entry->GetHttpStatusCode());
  EXPECT_EQ(old_navigation_entry->GetRedirectChain(),
            new_navigation_entry->GetRedirectChain());

  sessions::NavigationTaskId* old_navigation_task_id =
      sessions::NavigationTaskId::Get(old_navigation_entry.get());
  sessions::NavigationTaskId* new_navigation_task_id =
      sessions::NavigationTaskId::Get(new_navigation_entry.get());
  EXPECT_EQ(old_navigation_task_id->id(), new_navigation_task_id->id());
  EXPECT_EQ(old_navigation_task_id->parent_id(),
            new_navigation_task_id->parent_id());
  EXPECT_EQ(old_navigation_task_id->root_id(),
            new_navigation_task_id->root_id());

  TestData* test_data = static_cast<TestData*>(
      new_navigation_entry->GetUserData(kExtendedInfoKey1));
  ASSERT_TRUE(test_data);
  EXPECT_EQ(kExtendedInfoValue1, test_data->string);
  test_data = static_cast<TestData*>(
      new_navigation_entry->GetUserData(kExtendedInfoKey2));
  ASSERT_TRUE(test_data);
  EXPECT_EQ(kExtendedInfoValue2, test_data->string);
}

TEST_F(ContentSerializedNavigationBuilderTest, ToNavigationEntries) {
  content::BrowserTaskEnvironment test_environment;
  content::TestBrowserContext browser_context;

  // Make a NavigationEntry and serialize it twice into a vector.
  const std::unique_ptr<content::NavigationEntry> entry(
      MakeNavigationEntryForTest());
  std::vector<SerializedNavigationEntry> navigations;
  navigations.push_back(
      ContentSerializedNavigationBuilder::FromNavigationEntry(0, entry.get()));
  navigations.push_back(
      ContentSerializedNavigationBuilder::FromNavigationEntry(1, entry.get()));

  // Deserialize them via ToNavigationEntries().
  std::vector<std::unique_ptr<content::NavigationEntry>> restored_entries =
      ContentSerializedNavigationBuilder::ToNavigationEntries(navigations,
                                                              &browser_context);
  ASSERT_EQ(restored_entries.size(), 2ul);
  ASSERT_EQ(entry->GetURL(), restored_entries[0]->GetURL());
  ASSERT_EQ(entry->GetURL(), restored_entries[1]->GetURL());

  // Because the state of the two SerializedNavigationEntries was identical
  // (specifically, their item sequence numbers), the root
  // FrameNavigationEntries for the two restored entries should be de-duplicated
  // and shared. The url is stored on the root FrameNavigationEntry, so because
  // they are shared, calling SetURL() on one of the restored entries should
  // update the url on both.
  GURL new_url("http://www.url.com#ToNavigationEntries");
  restored_entries[0]->SetURL(new_url);
  EXPECT_EQ(new_url, restored_entries[0]->GetURL());
  EXPECT_EQ(new_url, restored_entries[1]->GetURL());
}

// Test that we can restore FrameNavigationEntries with matching item sequence
// numbers and unique names but different URLs, as these may be found in
// serialized sessions from builds prior to r894383.
// See https://crbug.com/1275257.
TEST_F(ContentSerializedNavigationBuilderTest, DeduplicateEntriesByURL) {
  content::BrowserTaskEnvironment test_environment;
  content::TestBrowserContext browser_context;

  // Make a NavigationEntry and serialize it several times into a vector.
  // Change the URL but not item sequence number for the last two entries, to
  // simulate a replaceState that was serialized from a build prior to r894383.
  const std::unique_ptr<content::NavigationEntry> entry(
      MakeNavigationEntryForTest());
  GURL url1("http://www.url.com/page1");
  entry->SetURL(url1);
  std::vector<SerializedNavigationEntry> navigations;
  navigations.push_back(
      ContentSerializedNavigationBuilder::FromNavigationEntry(0, entry.get()));
  navigations.push_back(
      ContentSerializedNavigationBuilder::FromNavigationEntry(1, entry.get()));
  GURL url2("http://www.url.com/page2");
  entry->SetURL(url2);
  navigations.push_back(
      ContentSerializedNavigationBuilder::FromNavigationEntry(2, entry.get()));
  navigations.push_back(
      ContentSerializedNavigationBuilder::FromNavigationEntry(3, entry.get()));

  // Deserialize them via ToNavigationEntries().
  std::vector<std::unique_ptr<content::NavigationEntry>> restored_entries =
      ContentSerializedNavigationBuilder::ToNavigationEntries(navigations,
                                                              &browser_context);
  ASSERT_EQ(restored_entries.size(), 4ul);
  ASSERT_EQ(url1, restored_entries[0]->GetURL());
  ASSERT_EQ(url1, restored_entries[1]->GetURL());
  ASSERT_EQ(url2, restored_entries[2]->GetURL());
  ASSERT_EQ(url2, restored_entries[3]->GetURL());

  // The root FrameNavigationEntries for the first two restored entries should
  // have been de-duplicated and shared, so changing the URL of one of them
  // should change the other, but not the third and fourth entries (which had a
  // different URL and thus aren't shared).
  GURL url3("http://www.url.com/page3");
  restored_entries[0]->SetURL(url3);
  EXPECT_EQ(url3, restored_entries[0]->GetURL());
  EXPECT_EQ(url3, restored_entries[1]->GetURL());
  EXPECT_EQ(url2, restored_entries[2]->GetURL());
  EXPECT_EQ(url2, restored_entries[3]->GetURL());

  // The third and fourth entries should have a shared FrameNavigationEntry as
  // well, such that updating one affects the other but not the first and second
  // entries.
  GURL url4("http://www.url.com/page4");
  restored_entries[3]->SetURL(url4);
  EXPECT_EQ(url3, restored_entries[0]->GetURL());
  EXPECT_EQ(url3, restored_entries[1]->GetURL());
  EXPECT_EQ(url4, restored_entries[2]->GetURL());
  EXPECT_EQ(url4, restored_entries[3]->GetURL());
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
