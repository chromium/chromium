// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_builder.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sessions/content/content_serialized_navigation_driver.h"
#include "components/sessions/content/extended_info_handler.h"
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

class TestExtendedInfoHandler : public ExtendedInfoHandler {
 public:
  explicit TestExtendedInfoHandler(const std::string& key) : key_(key) {}
  ~TestExtendedInfoHandler() override {}

  std::string GetExtendedInfo(
      const content::NavigationEntry& entry) const override {
    base::string16 data;
    entry.GetExtraData(key_, &data);
    return base::UTF16ToASCII(data);
  }

  void RestoreExtendedInfo(const std::string& info,
                           content::NavigationEntry* entry) override {
    entry->SetExtraData(key_, base::ASCIIToUTF16(info));
  }

 private:
  std::string key_;

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
  navigation_entry->SetVirtualURL(test_data::kVirtualURL);
  navigation_entry->SetTitle(test_data::kTitle);
  navigation_entry->SetPageState(
      content::PageState::CreateFromEncodedData(test_data::kEncodedPageState));
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
  return navigation_entry;
}

void SetExtendedInfoForTest(content::NavigationEntry* entry) {
  entry->SetExtraData(kExtendedInfoKey1,
                      base::ASCIIToUTF16(kExtendedInfoValue1));
  entry->SetExtraData(kExtendedInfoKey2,
                      base::ASCIIToUTF16(kExtendedInfoValue2));
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
          test_data::kIndex, *navigation_entry);

  EXPECT_EQ(test_data::kIndex, navigation.index());

  EXPECT_EQ(navigation_entry->GetUniqueID(), navigation.unique_id());
  EXPECT_EQ(test_data::kReferrerURL, navigation.referrer_url());
  EXPECT_EQ(test_data::kReferrerPolicy, navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_EQ(test_data::kEncodedPageState, navigation.encoded_page_state());
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
}

// Test effect of the navigation serialization options.
TEST_F(ContentSerializedNavigationBuilderTest,
       FromNavigationEntrySerializationOptions) {
  const std::unique_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& default_navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, *navigation_entry,
          ContentSerializedNavigationBuilder::DEFAULT);
  EXPECT_EQ(test_data::kEncodedPageState,
            default_navigation.encoded_page_state());

  const SerializedNavigationEntry& excluded_page_state_navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, *navigation_entry,
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
          test_data::kIndex, *old_navigation_entry);

  const std::unique_ptr<content::NavigationEntry> new_navigation_entry(
      ContentSerializedNavigationBuilder::ToNavigationEntry(&navigation,
                                                            nullptr));

  EXPECT_EQ(test_data::kReferrerURL, new_navigation_entry->GetReferrer().url);
  EXPECT_EQ(test_data::kReferrerPolicy,
            static_cast<int>(new_navigation_entry->GetReferrer().policy));
  EXPECT_EQ(test_data::kVirtualURL, new_navigation_entry->GetVirtualURL());
  EXPECT_EQ(test_data::kTitle, new_navigation_entry->GetTitle());
  EXPECT_EQ(test_data::kEncodedPageState,
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

  base::string16 extra_data;
  EXPECT_TRUE(
      new_navigation_entry->GetExtraData(kExtendedInfoKey1, &extra_data));
  EXPECT_EQ(kExtendedInfoValue1, base::UTF16ToASCII(extra_data));
  EXPECT_TRUE(
      new_navigation_entry->GetExtraData(kExtendedInfoKey2, &extra_data));
  EXPECT_EQ(kExtendedInfoValue2, base::UTF16ToASCII(extra_data));
}

TEST_F(ContentSerializedNavigationBuilderTest, SetPasswordState) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());

  EXPECT_EQ(SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
            GetPasswordStateFromNavigation(*entry));
  SetPasswordStateInNavigation(SerializedNavigationEntry::NO_PASSWORD_FIELD,
                               entry.get());
  EXPECT_EQ(SerializedNavigationEntry::NO_PASSWORD_FIELD,
            GetPasswordStateFromNavigation(*entry));
}

}  // namespace sessions
