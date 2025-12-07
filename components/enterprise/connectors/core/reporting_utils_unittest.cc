// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_utils.h"

#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "net/base/network_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kUsername[] = "username";
constexpr char kPasswordTrigger[] = "PASSWORD_ENTRY";

}  // namespace

TEST(ReportingUtilsTest, GetPasswordBreachEventReturnsValidEvent) {
  std::vector<std::pair<GURL, std::u16string>> identities = {
      {GURL("https://google.com/"), u"username"}};
  ReportingSettings settings;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  enabled_opt_in_events["passwordBreachEvent"].push_back("https://google.com/");
  settings.enabled_opt_in_events.insert(enabled_opt_in_events.begin(),
                                        enabled_opt_in_events.end());

  auto event = GetPasswordBreachEvent(kPasswordTrigger, identities, settings,
                                      "identifier", "profile_username");
  ASSERT_EQ(
      event->trigger(),
      chrome::cros::reporting::proto::PasswordBreachEvent::PASSWORD_ENTRY);
  auto identity = event->identities()[0];
  ASSERT_EQ(identity.url(), "https://google.com/");
  ASSERT_EQ(identity.username(), "*****");
  ASSERT_EQ(event->profile_identifier(), "identifier");
  ASSERT_EQ(event->profile_user_name(), "profile_username");
}

TEST(ReportingUtilsTest,
     GetPasswordBreachEventReturnsEmptyEventForNoMatchedUrl) {
  std::vector<std::pair<GURL, std::u16string>> identities = {
      {GURL("https://example.com/"), u"username"}};
  ReportingSettings settings;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  enabled_opt_in_events["passwordBreachEvent"].push_back("https://google.com/");
  settings.enabled_opt_in_events.insert(enabled_opt_in_events.begin(),
                                        enabled_opt_in_events.end());

  auto event = GetPasswordBreachEvent(kPasswordTrigger, identities, settings,
                                      "identifier", "profile_username");
  ASSERT_FALSE(event.has_value());
}

TEST(ReportingUtilsTest,
     GetPasswordBreachEventReturnsEmptyEventForEmptySettings) {
  std::vector<std::pair<GURL, std::u16string>> identities = {
      {GURL("https://google.com/"), u"username"}};
  ReportingSettings settings;

  auto event = GetPasswordBreachEvent(kPasswordTrigger, identities, settings,
                                      "identifier", "profile_username");
  ASSERT_FALSE(event.has_value());
}

TEST(ReportingUtilsTest, GetPasswordReuseEventWithWarning) {
  auto event = GetPasswordReuseEvent(
      /*url=*/GURL("https://google.com/"), /*user_name=*/kUsername,
      /*is_phishing_url=*/false, /*warning_shown=*/true,
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username");
  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.user_name(), kUsername);
  ASSERT_FALSE(event.is_phishing_url());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
}

TEST(ReportingUtilsTest, GetPasswordReuseEventWithoutWarning) {
  auto event = GetPasswordReuseEvent(
      /*url=*/GURL("https://google.com/"), /*user_name=*/kUsername,
      /*is_phishing_url=*/false, /*warning_shown=*/true,
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username");
  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.user_name(), kUsername);
  ASSERT_FALSE(event.is_phishing_url());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
}

TEST(ReportingUtilsTest, GetPasswordChangedEvent) {
  auto event =
      GetPasswordChangedEvent(kUsername, "identifier", "profile_username");
  ASSERT_EQ(event.user_name(), kUsername);
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
}

TEST(ReportingUtilsTest, GetLoginEvent) {
  url::SchemeHostPort federated_origin = url::SchemeHostPort();
  auto event = GetLoginEvent(/*url=*/GURL("https://google.com/"),
                             /*is_federated=*/federated_origin.IsValid(),
                             /*federated_origin=*/federated_origin,
                             /*username=*/u"username",
                             /*profile_identifier=*/"identifier",
                             /*profile_username=*/"profile_username");

  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_FALSE(event.is_federated());
  ASSERT_EQ(event.federated_origin(), "");
  ASSERT_EQ(event.login_user_name(), "*****");
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
}

TEST(ReportingUtilsTest, GetInterstitialEvent) {
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  auto event = GetInterstitialEvent(/*url=*/GURL("https://google.com/"),
                                    /*reason=*/"MALWARE", /*net_error_code=*/0,
                                    /*clicked_through=*/false,
                                    /*event_result=*/EventResult::WARNED,
                                    /*profile_identifier=*/"identifier",
                                    /*profile_username=*/"profile_username",
                                    /*referrer_chain*/ referrer_chain);

  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(
      event.reason(),
      chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent::MALWARE);
  ASSERT_EQ(event.net_error_code(), 0);
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    ASSERT_EQ(event.referrers_size(), 1);
    auto referrer = event.referrers()[0];
    ASSERT_EQ(referrer.url(), "https://referrer.com");
    ASSERT_EQ(referrer.ip(), "1.2.3.4");
  } else {
    ASSERT_EQ(event.referrers_size(), 0);
  }
}

TEST(ReportingUtilsTest, GetUrlFilteringInterstitialEvent) {
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");

  auto event = GetUrlFilteringInterstitialEvent(
      /*url=*/GURL("https://filteredurl.com"),
      /*threat_type=*/"ENTERPRISE_BLOCKED_SEEN", /*response=*/response,
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username",
      /*active_user=*/"active_user@example.com",
      /*referrer_chain=*/referrer_chain);

  ASSERT_EQ(event.url(), "https://filteredurl.com/");
  ASSERT_FALSE(event.clicked_through());
  ASSERT_EQ(event.threat_type(),
            chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
                ENTERPRISE_BLOCKED_SEEN);
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);
  ASSERT_EQ(event.triggered_rule_info_size(), 1);

  chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule_info =
      event.mutable_triggered_rule_info()->at(0);
  ASSERT_EQ(triggered_rule_info.rule_name(), "test rule name");
  ASSERT_EQ(triggered_rule_info.rule_id(), 123);
  ASSERT_EQ(triggered_rule_info.url_category(), "test rule category");
  ASSERT_EQ(triggered_rule_info.action(),
            chrome::cros::reporting::proto::TriggeredRuleInfo::BLOCK);
  ASSERT_FALSE(triggered_rule_info.has_watermarking());
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
  ASSERT_EQ(event.web_app_signed_in_account(), "active_user@example.com");

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    ASSERT_EQ(event.referrers_size(), 1);
    auto referrer = event.referrers()[0];
    ASSERT_EQ(referrer.url(), "https://referrer.com");
    ASSERT_EQ(referrer.ip(), "1.2.3.4");
  } else {
    ASSERT_EQ(event.referrers_size(), 0);
  }
}

TEST(ReportingUtilsTest, GetBrowserCrashEvent) {
  auto event =
      GetBrowserCrashEvent(/*channel=*/"canary", /*version=*/"100.0.0000.000",
                           /*report_id=*/"123", /*platform=*/"Windows");

  ASSERT_EQ(event.channel(), "canary");
  ASSERT_EQ(event.version(), "100.0.0000.000");
  ASSERT_EQ(event.report_id(), "123");
  ASSERT_EQ(event.platform(), "Windows");
}

TEST(ReportingUtilsTest, GetUnscannedFileEvent) {
  auto event = GetUnscannedFileEvent(
      /*url=*/GURL("https://google.com/"), /*tab_url=*/GURL("about:blank"),
      /*source=*/"source", /*destination=*/"destination",
      /*file_name=*/"encrypted.zip",
      /*download_digest_sha256=*/"sha256_of_data",
      /*mime_type=*/"application/zip", /*trigger=*/"FILE_UPLOAD",
      /*reason=*/"FILE_PASSWORD_PROTECTED",
      /*content_transfer_method=*/"CONTENT_TRANSFER_METHOD_DRAG_AND_DROP",
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username", /*content_size=*/-1,
      /*event_result=*/EventResult::ALLOWED);

  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.tab_url(), "about:blank");
  ASSERT_EQ(event.source(), "source");
  ASSERT_EQ(event.destination(), "destination");
  ASSERT_EQ(event.file_name(), "encrypted.zip");
  ASSERT_EQ(event.download_digest_sha_256(), "sha256_of_data");
  ASSERT_EQ(event.content_type(), "application/zip");
  ASSERT_EQ(
      event.trigger(),
      chrome::cros::reporting::proto::DataTransferEventTrigger::FILE_UPLOAD);
  ASSERT_EQ(event.unscanned_reason(),
            chrome::cros::reporting::proto::UnscannedFileEvent::
                FILE_PASSWORD_PROTECTED);
  ASSERT_EQ(
      event.content_transfer_method(),
      chrome::cros::reporting::proto::CONTENT_TRANSFER_METHOD_DRAG_AND_DROP);
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
  ASSERT_FALSE(event.content_size());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);
}

TEST(ReportingUtilsTest, GetDlpSensitiveDataEvent) {
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  FrameUrlChain frame_url_chain;
  *frame_url_chain.Add() = "https://frame1.com/";
  *frame_url_chain.Add() = "https://frame2.com/";

  ContentAnalysisResponse response;
  response.set_request_token("123");
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* rule = result->add_triggered_rules();
  rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  rule->set_rule_name("fake rule");
  rule->set_rule_id("12345");
  rule->set_url_category("test rule category");

  auto event = GetDlpSensitiveDataEvent(
      /*url=*/GURL("https://google.com/"), /*tab_url=*/GURL("about:blank"),
      /*source=*/"source", /*destination=*/"destination",
      /*file_name=*/"encrypted.zip",
      /*download_digest_sha256=*/"sha256_of_data",
      /*mime_type=*/"application/zip", /*trigger=*/"FILE_UPLOAD",
      /*scan_id=*/"123",
      /*content_transfer_method=*/"CONTENT_TRANSFER_METHOD_DRAG_AND_DROP",
      /*source_active_user_email=*/"source@gmail.com",
      /*content_area_account_email=*/"content@gmail.com",
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username",
      /*user_justification*/ u"justification",
      /*content_size=*/-1,
      /*result=*/*result,
      /*referrer_chain=*/referrer_chain,
      /*frame_url_chain=*/frame_url_chain,
      /*event_result=*/EventResult::BLOCKED);

  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.tab_url(), "about:blank");
  ASSERT_EQ(event.source(), "source");
  ASSERT_EQ(event.destination(), "destination");
  ASSERT_EQ(event.file_name(), "encrypted.zip");
  ASSERT_EQ(event.download_digest_sha_256(), "sha256_of_data");
  ASSERT_EQ(event.content_type(), "application/zip");
  ASSERT_EQ(
      event.trigger(),
      chrome::cros::reporting::proto::DataTransferEventTrigger::FILE_UPLOAD);
  ASSERT_EQ(event.scan_id(), "123");
  ASSERT_EQ(
      event.content_transfer_method(),
      chrome::cros::reporting::proto::CONTENT_TRANSFER_METHOD_DRAG_AND_DROP);
  ASSERT_EQ(event.source_web_app_signed_in_account(), "source@gmail.com");
  ASSERT_EQ(event.web_app_signed_in_account(), "content@gmail.com");
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
  ASSERT_EQ(event.user_justification(), "justification");
  ASSERT_FALSE(event.content_size());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);
  ASSERT_FALSE(event.clicked_through());

  ASSERT_EQ(event.triggered_rule_info_size(), 1);
  auto triggered_rule = event.triggered_rule_info()[0];
  ASSERT_EQ(triggered_rule.rule_id(), 12345);
  ASSERT_EQ(triggered_rule.url_category(), "test rule category");
  ASSERT_EQ(triggered_rule.rule_name(), "fake rule");
  ASSERT_EQ(triggered_rule.action(),
            chrome::cros::reporting::proto::TriggeredRuleInfo::BLOCK);

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    ASSERT_EQ(event.referrers_size(), 1);
    auto referrer = event.referrers()[0];
    ASSERT_EQ(referrer.url(), "https://referrer.com");
    ASSERT_EQ(referrer.ip(), "1.2.3.4");
  } else {
    ASSERT_EQ(event.referrers_size(), 0);
  }

  ASSERT_EQ(event.iframe_urls_size(), 2);
  ASSERT_EQ(event.iframe_urls()[0], "https://frame1.com/");
  ASSERT_EQ(event.iframe_urls()[1], "https://frame2.com/");
}

TEST(ReportingUtilsTest, GetDangerousDownloadEvent) {
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  FrameUrlChain frame_url_chain;
  *frame_url_chain.Add() = "https://frame1.com/";
  *frame_url_chain.Add() = "https://frame2.com/";

  auto event = GetDangerousDownloadEvent(
      /*url=*/GURL("https://google.com/"), /*tab_url=*/GURL("about:blank"),
      /*source=*/"source", /*destination=*/"destination",
      /*file_name=*/"encrypted.zip",
      /*download_digest_sha256=*/"sha256_of_data",
      /*threat_type=*/"DANGEROUS_ACCOUNT_COMPROMISE",
      /*mime_type=*/"application/zip", /*trigger=*/"FILE_DOWNLOAD",
      /*scan_id=*/"123",
      /*content_transfer_method=*/"",
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username", /*content_size=*/-1,
      /*referrer_chain=*/referrer_chain,
      /*frame_url_chain=*/frame_url_chain,
      /*event_result=*/EventResult::BLOCKED);

  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.tab_url(), "about:blank");
  ASSERT_EQ(event.source(), "source");
  ASSERT_EQ(event.destination(), "destination");
  ASSERT_EQ(event.file_name(), "encrypted.zip");
  ASSERT_EQ(event.download_digest_sha256(), "sha256_of_data");
  ASSERT_EQ(event.content_type(), "application/zip");
  ASSERT_EQ(
      event.trigger(),
      chrome::cros::reporting::proto::DataTransferEventTrigger::FILE_DOWNLOAD);
  ASSERT_EQ(event.scan_id(), "123");
  ASSERT_EQ(
      event.threat_type(),
      chrome::cros::reporting::proto ::SafeBrowsingDangerousDownloadEvent::
          DANGEROUS_ACCOUNT_COMPROMISE);
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
  ASSERT_FALSE(event.content_size());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);
  ASSERT_FALSE(event.clicked_through());

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    ASSERT_EQ(event.referrers_size(), 1);
    auto referrer = event.referrers()[0];
    ASSERT_EQ(referrer.url(), "https://referrer.com");
    ASSERT_EQ(referrer.ip(), "1.2.3.4");
  } else {
    ASSERT_EQ(event.referrers_size(), 0);
  }

  ASSERT_EQ(event.iframe_urls_size(), 2);
  ASSERT_EQ(event.iframe_urls()[0], "https://frame1.com/");
  ASSERT_EQ(event.iframe_urls()[1], "https://frame2.com/");
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
TEST(ReportingUtilsTest, GetDataControlsSensitiveDataEvent) {
  data_controls::Verdict::TriggeredRules triggered_rules = {
      {{0, true}, {"1", "rule_1_name"}}};

  auto event = GetDataControlsSensitiveDataEvent(
      /*url=*/GURL("https://google.com/"), /*tab_url=*/GURL("about:blank"),
      /*source=*/"CLIPBOARD", /*destination=*/"about:blank",
      /*mime_type=*/"text/plain", /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*source_active_user_email=*/"source@gmail.com",
      /*content_area_account_email=*/"content@gmail.com",
      /*profile_identifier=*/"identifier",
      /*profile_username=*/"profile_username", /*content_size=*/-1,
      /*triggered_rules*/ triggered_rules,
      /*event_result=*/EventResult::BLOCKED);

  ASSERT_EQ(event.url(), "https://google.com/");
  ASSERT_EQ(event.tab_url(), "about:blank");
  ASSERT_EQ(event.source(), "CLIPBOARD");
  ASSERT_EQ(event.destination(), "about:blank");
  ASSERT_EQ(event.content_type(), "text/plain");
  ASSERT_EQ(event.trigger(), chrome::cros::reporting::proto::
                                 DataTransferEventTrigger::WEB_CONTENT_UPLOAD);
  ASSERT_EQ(event.source_web_app_signed_in_account(), "source@gmail.com");
  ASSERT_EQ(event.web_app_signed_in_account(), "content@gmail.com");
  ASSERT_EQ(event.profile_identifier(), "identifier");
  ASSERT_EQ(event.profile_user_name(), "profile_username");
  ASSERT_FALSE(event.content_size());
  ASSERT_EQ(event.event_result(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);

  ASSERT_EQ(event.triggered_rule_info_size(), 1);
  auto triggered_rule = event.triggered_rule_info()[0];
  ASSERT_EQ(triggered_rule.rule_id(), 1);
  ASSERT_EQ(triggered_rule.rule_name(), "rule_1_name");
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

TEST(ReportingUtilsTest, TestEventLocalIp) {
  std::vector<std::string> local_ips = GetLocalIpAddresses();
  // TODO(crbug.com//394602691): Remove Android build exclusion once IP address
  // support becomes a requirement for Android devices.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(local_ips.empty());
#endif
  for (const auto& ip_address : local_ips) {
    std::optional<net::IPAddress> local_ip =
        net::IPAddress::FromIPLiteral(ip_address);
    EXPECT_TRUE(local_ip->IsValid());
    EXPECT_FALSE(local_ip->IsZero());
  }
}

TEST(ReportingUtilsTest, TestMaskUserName) {
  EXPECT_EQ(MaskUsername(u"fakeuser"), "*****");
  EXPECT_EQ(MaskUsername(u"fakeuser@gmail.com"), "*****@gmail.com");
}

TEST(ReportingUtilsTest, TestUrlMatchingForOptInEventReturnsTrue) {
  ReportingSettings settings;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  enabled_opt_in_events["passwordBreachEvent"].push_back("*");
  settings.enabled_opt_in_events.insert(enabled_opt_in_events.begin(),
                                        enabled_opt_in_events.end());

  auto url_matcher = CreateURLMatcherForOptInEvent(std::move(settings),
                                                   kKeyPasswordBreachEvent);
  EXPECT_TRUE(IsUrlMatched(url_matcher.get(), GURL("gmail.com")));
}

TEST(ReportingUtilsTest, TestUrlMatchingForOptInEventReturnsFalse) {
  ReportingSettings settings;
  std::map<std::string, std::vector<std::string>> enabled_opt_in_events;
  enabled_opt_in_events["passwordBreachEvent"].push_back("https://google.com/");
  settings.enabled_opt_in_events.insert(enabled_opt_in_events.begin(),
                                        enabled_opt_in_events.end());

  auto url_matcher = CreateURLMatcherForOptInEvent(std::move(settings),
                                                   kKeyPasswordBreachEvent);
  EXPECT_FALSE(IsUrlMatched(url_matcher.get(), GURL("gmail.com")));
}

TEST(ReportingUtilsTest, TestAddReferrerChainToEvent) {
  google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
      referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  base::Value::Dict event;
  AddReferrerChainToEvent(referrer_chain, event);
  EXPECT_EQ(event.size(), 1u);
  EXPECT_EQ(event.FindList(kKeyReferrers)->size(), 1u);
}

TEST(ReportingUtilsTest, TestEmptyReferrerChainAdded) {
  google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
      referrer_chain;
  base::Value::Dict event;
  AddReferrerChainToEvent(referrer_chain, event);
  EXPECT_EQ(event.size(), 1u);
  EXPECT_TRUE(event.contains(kKeyReferrers));
  EXPECT_TRUE(event.FindList(kKeyReferrers)->empty());
}

TEST(ReportingUtilsTest, TestAddFrameUrlChainToEvent) {
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain;
  *frame_url_chain.Add() = "https://frame1.com/";
  *frame_url_chain.Add() = "https://frame2.com/";

  base::Value::Dict event;
  AddFrameUrlChainToEvent(frame_url_chain, event);

  const base::Value::List* iframe_urls = event.FindList(kKeyIframeUrls);
  ASSERT_TRUE(iframe_urls);
  ASSERT_EQ(iframe_urls->size(), 2u);
  EXPECT_EQ((*iframe_urls)[0].GetString(), "https://frame1.com/");
  EXPECT_EQ((*iframe_urls)[1].GetString(), "https://frame2.com/");
}

}  // namespace enterprise_connectors
