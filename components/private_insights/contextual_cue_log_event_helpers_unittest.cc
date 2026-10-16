// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/contextual_cue_log_event_helpers.h"

#include <vector>

#include "base/test/values_test_util.h"
#include "components/metrics/version_utils.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/private_insights/events/coarse_system_profile.pb.h"
#include "components/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace private_insights {
namespace {

std::optional<std::pair<std::string, std::string>> ExtractFromPageInfo(
    const events::ContextualCueLogEvent::PageInfo& page_info) {
  return std::make_pair(page_info.url(), page_info.title());
}

}  // namespace

TEST(ContextualCueLogEventHelpersTest, SerializeEmptyList) {
  std::vector<events::ContextualCueLogEvent::PageInfo> empty_list;
  EXPECT_EQ("[]",
            SerializeCollectionToPageInfoJson(empty_list, ExtractFromPageInfo));
}

TEST(ContextualCueLogEventHelpersTest, SerializeOneItemList) {
  events::ContextualCueLogEvent::PageInfo page_info;
  page_info.set_url("http://url1.com");
  page_info.set_title("Title 1");
  std::vector<events::ContextualCueLogEvent::PageInfo> list = {page_info};
  EXPECT_THAT(SerializeCollectionToPageInfoJson(list, ExtractFromPageInfo),
              base::test::IsJson(
                  "[{\"url\":\"http://url1.com\",\"title\":\"Title 1\"}]"));
}

TEST(ContextualCueLogEventHelpersTest, SerializeMultipleItemsList) {
  events::ContextualCueLogEvent::PageInfo page_info1;
  page_info1.set_url("http://url1.com");
  page_info1.set_title("Title 1");
  events::ContextualCueLogEvent::PageInfo page_info2;
  page_info2.set_url("http://url2.com");
  page_info2.set_title("Title 2");
  events::ContextualCueLogEvent::PageInfo page_info3;
  page_info3.set_url("http://url3.com");
  page_info3.set_title("Title 3");

  std::vector<events::ContextualCueLogEvent::PageInfo> list = {
      page_info1, page_info2, page_info3};
  EXPECT_THAT(SerializeCollectionToPageInfoJson(list, ExtractFromPageInfo),
              base::test::IsJson(
                  "[{\"url\":\"http://url1.com\",\"title\":\"Title 1\"},"
                  "{\"url\":\"http://url2.com\",\"title\":\"Title 2\"},"
                  "{\"url\":\"http://url3.com\",\"title\":\"Title 3\"}]"));
}

TEST(ContextualCueLogEventHelpersTest, SerializeSpecialCharactersList) {
  events::ContextualCueLogEvent::PageInfo page_info1;
  page_info1.set_url("http://url1.com/a,b");
  page_info1.set_title("Title 1, with comma");
  events::ContextualCueLogEvent::PageInfo page_info2;
  page_info2.set_url("http://url2.com/\"quotes\"");
  page_info2.set_title("Title 2 \"with quotes\"");
  events::ContextualCueLogEvent::PageInfo page_info3;
  page_info3.set_url("http://url3.com/newline");
  page_info3.set_title("Title 3\nwith newline");

  std::vector<events::ContextualCueLogEvent::PageInfo> list = {
      page_info1, page_info2, page_info3};
  std::string json =
      SerializeCollectionToPageInfoJson(list, ExtractFromPageInfo);
  EXPECT_THAT(json,
              base::test::IsJson(
                  "[{\"url\":\"http://url1.com/a,b\",\"title\":\"Title 1, with "
                  "comma\"},"
                  "{\"url\":\"http://url2.com/"
                  "\\\"quotes\\\"\",\"title\":\"Title 2 \\\"with quotes\\\"\"},"
                  "{\"url\":\"http://url3.com/newline\",\"title\":\"Title "
                  "3\\nwith newline\"}]"));
}

TEST(ContextualCueLogEventHelpersTest, ExtractFromProtoTab_EmptyTab) {
  // Given
  optimization_guide::proto::Tab empty_tab;
  std::vector<optimization_guide::proto::Tab> background_tabs = {empty_tab};

  // When
  std::string recent_pages =
      SerializeCollectionToPageInfoJson(background_tabs, ExtractFromProtoTab);

  // Then
  // An empty proto tab has empty strings for URL and Title.
  // The extractor returns them, and they are serialized.
  EXPECT_EQ("[{\"title\":\"\",\"url\":\"\"}]", recent_pages);
}

// Test that proto tabs serialize with their URL and title verbatim.
TEST(ContextualCueLogEventHelpersTest, ExtractFromProtoTab_PopulatedTabs) {
  optimization_guide::proto::Tab tab1;
  tab1.set_url("https://bg.com");
  tab1.set_title("Bg Title");
  optimization_guide::proto::Tab tab2;
  tab2.set_url("https://other.com/");
  tab2.set_title("Other Title");
  std::vector<optimization_guide::proto::Tab> background_tabs = {tab1, tab2};
  EXPECT_EQ(
      "[{\"title\":\"Bg Title\",\"url\":\"https://bg.com\"},"
      "{\"title\":\"Other Title\",\"url\":\"https://other.com/\"}]",
      SerializeCollectionToPageInfoJson(background_tabs, ExtractFromProtoTab));
}

// Test that the common system profile carries the running version, milestone
// and the UMA operating system name, and leaves the embedder fields unset.
TEST(ContextualCueLogEventHelpersTest, PopulateCommonSystemProfile) {
  events::CoarseSystemProfile system_profile;
  PopulateCommonSystemProfile(&system_profile);

  EXPECT_EQ(version_info::GetVersionNumber(), system_profile.chrome_version());
  EXPECT_EQ(version_info::GetMajorVersionNumber(), system_profile.milestone());
  EXPECT_EQ(metrics::GetOperatingSystemName(), system_profile.platform());
  EXPECT_FALSE(system_profile.chrome_version().empty());
  EXPECT_FALSE(system_profile.milestone().empty());
  EXPECT_FALSE(system_profile.platform().empty());
  EXPECT_TRUE(system_profile.chrome_channel().empty());
  EXPECT_TRUE(system_profile.language().empty());
  EXPECT_TRUE(system_profile.country().empty());
}

// Test that the active page holds the canonical URL spec and the title.
TEST(ContextualCueLogEventHelpersTest, PopulateActivePage) {
  events::ContextualCueLogEvent::PageInfo active_page;
  PopulateActivePage(GURL("https://active.com"), "Active Title", &active_page);

  EXPECT_EQ("https://active.com/", active_page.url());
  EXPECT_EQ("Active Title", active_page.title());
  EXPECT_EQ("active.com", active_page.hostname());
}

// Verifies that `active_page.hostname` holds the host component of the URL,
// as returned by `GURL::host()`.
struct HostTestCase {
  const char* test_name;
  const char* url;
  const char* expected_host;
};

const HostTestCase kHostTestCases[] = {
    {"BareDomain", "https://active.com/", "active.com"},
    // Subdomains are preserved; this is what distinguishes the host from an
    // eTLD+1.
    {"Subdomain", "https://www.active.com/", "www.active.com"},
    {"NestedSubdomains", "https://www.foo.bar.active.com/",
     "www.foo.bar.active.com"},
    {"MultiLevelPublicSuffix", "https://a.b.co.uk/", "a.b.co.uk"},
    {"PrivateRegistry", "https://myblog.blogspot.com/", "myblog.blogspot.com"},
    // Port, path, query, and fragment are not part of the host.
    {"PortPathQueryFragment", "https://www.active.com:8080/a/b?q=1#frag",
     "www.active.com"},
    // Userinfo is not part of the host.
    {"Userinfo", "https://user:pass@www.active.com/", "www.active.com"},
    // GURL canonicalizes the host to lowercase.
    {"Uppercase", "https://WWW.Active.COM/", "www.active.com"},
    // Hosts that have no registrable domain are still returned verbatim.
    {"IpAddress", "http://192.168.0.1/", "192.168.0.1"},
    {"SingleComponentHost", "http://localhost:3000/", "localhost"},
    // URLs without a host yield an empty string.
    {"FileUrl", "file:///tmp/bar.html", ""},
    {"AboutBlank", "about:blank", ""},
};

class ContextualCueLogEventHelpersHostTest
    : public testing::TestWithParam<HostTestCase> {};

TEST_P(ContextualCueLogEventHelpersHostTest, PopulateActivePagePopulatesHost) {
  // Given
  const HostTestCase& test_case = GetParam();
  events::ContextualCueLogEvent::PageInfo active_page;

  // When
  PopulateActivePage(GURL(test_case.url), "Active Title", &active_page);

  // Then
  EXPECT_EQ(test_case.expected_host, active_page.hostname());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualCueLogEventHelpersHostTest,
                         testing::ValuesIn(kHostTestCases),
                         [](const testing::TestParamInfo<HostTestCase>& info) {
                           return info.param.test_name;
                         });

// Test the mapping from the server cue to `cue_details`: anchored message
// text to `suggestion_text`, action text to `button_text`, Gemini surface
// prompt to `prompt`, and the caller-provided `cuj` and `promoted_feature`.
TEST(ContextualCueLogEventHelpersTest, PopulateCueDetails) {
  optimization_guide::proto::ContextualCue cue;
  cue.mutable_anchored_message_cue()->set_anchored_message_text("Compare");
  cue.mutable_anchored_message_cue()->set_action_text("Compare prices");
  cue.mutable_gemini_in_chrome_surface()->set_prompt("Compare these products");

  events::ContextualCueLogEvent::CueDetails cue_details;
  PopulateCueDetails(cue, "SHOPPING_COMPARE", "Glic", &cue_details);

  EXPECT_EQ("SHOPPING_COMPARE", cue_details.cuj_type());
  EXPECT_EQ("Compare", cue_details.suggestion_text());
  EXPECT_EQ("Compare prices", cue_details.button_text());
  EXPECT_EQ("Compare these products", cue_details.prompt());
  EXPECT_EQ("Glic", cue_details.promoted_feature());
}

// Test that a cue without a Gemini surface leaves `prompt` unset.
TEST(ContextualCueLogEventHelpersTest, PopulateCueDetails_NoGeminiSurface) {
  optimization_guide::proto::ContextualCue cue;
  cue.mutable_anchored_message_cue()->set_anchored_message_text("Compare");

  events::ContextualCueLogEvent::CueDetails cue_details;
  PopulateCueDetails(cue, "SHOPPING_COMPARE", "Glic", &cue_details);

  EXPECT_EQ("Compare", cue_details.suggestion_text());
  EXPECT_TRUE(cue_details.prompt().empty());
}

// Test that a contextual search cue uses its query as the `prompt`.
TEST(ContextualCueLogEventHelpersTest, PopulateCueDetails_ContextualSearch) {
  optimization_guide::proto::ContextualCue cue;
  cue.mutable_anchored_message_cue()->set_anchored_message_text("Message");
  cue.mutable_contextual_search_surface()->set_query("search query");

  events::ContextualCueLogEvent::CueDetails cue_details;
  PopulateCueDetails(cue, "LEARNING", "ContextualSearch", &cue_details);

  EXPECT_EQ("search query", cue_details.prompt());
  EXPECT_EQ("ContextualSearch", cue_details.promoted_feature());
}

}  // namespace private_insights
