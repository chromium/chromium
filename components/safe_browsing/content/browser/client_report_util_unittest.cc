// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_report_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kPhishingUrl[] = "https://phishing.com/";

namespace safe_browsing {

class ClientReportUtilTest : public testing::Test {
 protected:
  ClientReportUtilTest() = default;

  security_interstitials::UnsafeResource GetPhishingResource() {
    security_interstitials::UnsafeResource resource =
        security_interstitials::UnsafeResource();
    resource.url = GURL(kPhishingUrl);
    resource.threat_type = SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    return resource;
  }

  std::unique_ptr<security_interstitials::InterstitialInteractionMap>
  SetUpInterstitialInteractionMap(const int kExpectedShowMoreEvents,
                                  const int kExpectedOpenHelpCenter,
                                  const int kExpectedProceed) {
    auto interactions_map =
        std::make_unique<security_interstitials::InterstitialInteractionMap>();
    interactions_map->insert_or_assign(
        security_interstitials::SecurityInterstitialCommand::
            CMD_SHOW_MORE_SECTION,
        security_interstitials::InterstitialInteractionDetails(
            kExpectedShowMoreEvents,
            base::Time::Now().InMillisecondsSinceUnixEpoch() - 2000,
            base::Time::Now().InMillisecondsSinceUnixEpoch()));
    interactions_map->insert_or_assign(
        security_interstitials::SecurityInterstitialCommand::
            CMD_OPEN_HELP_CENTER,
        security_interstitials::InterstitialInteractionDetails(
            kExpectedOpenHelpCenter,
            base::Time::Now().InMillisecondsSinceUnixEpoch() - 1000,
            base::Time::Now().InMillisecondsSinceUnixEpoch()));
    int64_t time_now = base::Time::Now().InMillisecondsSinceUnixEpoch();
    interactions_map->insert_or_assign(
        security_interstitials::SecurityInterstitialCommand::CMD_PROCEED,
        security_interstitials::InterstitialInteractionDetails(
            kExpectedProceed, time_now, time_now));
    return interactions_map;
  }

  void VerifyInterstitialInteraction(
      const safe_browsing::ClientSafeBrowsingReportRequest& report,
      ClientSafeBrowsingReportRequest::InterstitialInteraction::
          SecurityInterstitialInteraction expected_interaction,
      int expected_count) {
    for (auto interaction : report.interstitial_interactions()) {
      if (interaction.security_interstitial_interaction() ==
          expected_interaction) {
        EXPECT_EQ(interaction.occurrence_count(), expected_count);
        EXPECT_EQ(interaction.first_interaction_timestamp_msec() ==
                      interaction.last_interaction_timestamp_msec(),
                  expected_count == 1);
      }
    }
  }

  void VerifyInterstitialInteractionsReport(
      const safe_browsing::ClientSafeBrowsingReportRequest& report,
      const int kExpectedShowMoreEvents,
      const int kExpectedOpenHelpCenter,
      const int kExpectedProceed) {
    EXPECT_EQ(report.interstitial_interactions_size(), 3);
    VerifyInterstitialInteraction(
        report,
        ClientSafeBrowsingReportRequest::InterstitialInteraction::
            CMD_SHOW_MORE_SECTION,
        kExpectedShowMoreEvents);
    VerifyInterstitialInteraction(
        report,
        ClientSafeBrowsingReportRequest::InterstitialInteraction::
            CMD_OPEN_HELP_CENTER,
        kExpectedOpenHelpCenter);
    VerifyInterstitialInteraction(
        report,
        ClientSafeBrowsingReportRequest::InterstitialInteraction::CMD_PROCEED,
        kExpectedProceed);
  }
};

TEST_F(ClientReportUtilTest, FillBasicReport) {
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  client_report_utils::FillReportBasicResourceDetails(report.get(),
                                                      GetPhishingResource());
  ASSERT_EQ(report->url(), kPhishingUrl);
  ASSERT_EQ(report->type(),
            safe_browsing::ClientSafeBrowsingReportRequest::URL_PHISHING);
  ASSERT_EQ(report->url_request_destination(),
            ClientSafeBrowsingReportRequest::DOCUMENT);
  ASSERT_TRUE(report->has_client_properties());
  ASSERT_TRUE(report->client_properties().has_url_api_type());
  ASSERT_TRUE(report->client_properties().has_is_async_check());
  ASSERT_EQ(
      report->client_properties().url_api_type(),
      ClientSafeBrowsingReportRequest::SAFE_BROWSING_URL_API_TYPE_UNSPECIFIED);
  ASSERT_FALSE(report->client_properties().is_async_check());
}

TEST_F(ClientReportUtilTest, FillInterstitialInteractionsOfReport) {
  const int kExpectedShowMoreEvents = 3;
  const int kExpectedOpenHelpCenter = 2;
  const int kExpectedProceed = 1;
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  client_report_utils::FillReportBasicResourceDetails(report.get(),
                                                      GetPhishingResource());
  auto interactions_map = SetUpInterstitialInteractionMap(
      kExpectedShowMoreEvents, kExpectedOpenHelpCenter, kExpectedProceed);
  client_report_utils::FillInterstitialInteractionsHelper(
      report.get(), interactions_map.get());
  VerifyInterstitialInteractionsReport(*report.get(), kExpectedShowMoreEvents,
                                       kExpectedOpenHelpCenter,
                                       kExpectedProceed);
}

}  // namespace safe_browsing
