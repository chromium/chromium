// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "components/safe_browsing/core/browser/ping_manager.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "google_apis/google_api_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using safe_browsing::HitReport;
using safe_browsing::ThreatSource;

namespace safe_browsing {

class PingManagerTest : public testing::Test {
 public:
  PingManagerTest() {}

 protected:
  void SetUp() override {
    std::string key = google_apis::GetAPIKey();
    if (!key.empty()) {
      key_param_ = base::StringPrintf(
          "&key=%s", base::EscapeQueryParamValue(key, true).c_str());
    }

    ping_manager_.reset(new PingManager(
        safe_browsing::GetTestV4ProtocolConfig(), nullptr, nullptr,
        base::BindRepeating([]() { return false; }), nullptr, nullptr,
        base::NullCallback(), base::NullCallback()));
  }

  PingManager* ping_manager() { return ping_manager_.get(); }

  std::string key_param_;
  std::unique_ptr<PingManager> ping_manager_;
};

TEST_F(PingManagerTest, TestSafeBrowsingHitUrl) {
  HitReport base_hp;
  base_hp.malicious_url = GURL("http://malicious.url.com");
  base_hp.page_url = GURL("http://page.url.com");
  base_hp.referrer_url = GURL("http://referrer.url.com");

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_subresource = true;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;

    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=1&enh=1&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = false;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&enh=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_BINARY_MALWARE;
    hp.threat_source = ThreatSource::REMOTE;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = true;
    hp.is_subresource = false;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=binurlhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=rem&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = false;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=phishcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  // Same as above, but add population_id
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    hp.population_id = "foo bar";
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0&up=foo+bar",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  // Threat source is real time check.
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::REAL_TIME_CHECK;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&enh=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=rt&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }
}

TEST_F(PingManagerTest, TestThreatDetailsUrl) {
  EXPECT_EQ(
      "https://safebrowsing.google.com/safebrowsing/clientreport/malware?"
      "client=unittest&appver=1.0&pver=4.0" +
          key_param_,
      ping_manager()->ThreatDetailsUrl().spec());
}

TEST_F(PingManagerTest, TestReportThreatDetails_EmptyReport) {
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  PingManager::ReportThreatDetailsResult result =
      ping_manager()->ReportThreatDetails(std::move(report));
  EXPECT_EQ(result, PingManager::ReportThreatDetailsResult::EMPTY_REPORT);
}

TEST_F(PingManagerTest, TestSanitizeThreatDetailsReport) {
  // Blank report.
  {
    std::unique_ptr<ClientSafeBrowsingReportRequest> report =
        std::make_unique<ClientSafeBrowsingReportRequest>();
    ping_manager()->SanitizeThreatDetailsReport(report.get());
    EXPECT_EQ(report->url(), "");
  }
  // One field needs sanitizing.
  {
    std::unique_ptr<ClientSafeBrowsingReportRequest> report =
        std::make_unique<ClientSafeBrowsingReportRequest>();
    report->set_url("http://user1:pass2@some.url.com/");
    report->set_page_url("http://some.page.url.com/");
    ping_manager()->SanitizeThreatDetailsReport(report.get());
    EXPECT_EQ(report->url(), "http://some.url.com/");
    EXPECT_EQ(report->page_url(), "http://some.page.url.com/");
  }
  // Multiple fields need sanitizing.
  {
    std::unique_ptr<ClientSafeBrowsingReportRequest> report =
        std::make_unique<ClientSafeBrowsingReportRequest>();
    report->set_url("http://user1:pass2@some.url.com/");
    report->set_page_url("http://u1:p2@some.page.url.com/");
    report->set_referrer_url("http://a:b@some.referrer.url.com/");
    report->add_resources()->set_url("http://c:d@first.resource.com/");
    report->add_resources();  // second resource has blank URL
    report->add_resources()->set_url("http://e:f@third.resource.com/");
    ping_manager()->SanitizeThreatDetailsReport(report.get());
    EXPECT_EQ(report->url(), "http://some.url.com/");
    EXPECT_EQ(report->page_url(), "http://some.page.url.com/");
    EXPECT_EQ(report->referrer_url(), "http://some.referrer.url.com/");
    EXPECT_EQ(report->resources(0).url(), "http://first.resource.com/");
    EXPECT_EQ(report->resources(1).url(), "");
    EXPECT_EQ(report->resources(2).url(), "http://third.resource.com/");
  }
}

TEST_F(PingManagerTest, TestSanitizeHitReport) {
  // Blank report.
  {
    std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
    ping_manager()->SanitizeHitReport(hit_report.get());
    EXPECT_EQ(hit_report->malicious_url.spec(), "");
    EXPECT_EQ(hit_report->page_url.spec(), "");
    EXPECT_EQ(hit_report->referrer_url.spec(), "");
  }
  // One field needs sanitizing.
  {
    std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
    hit_report->malicious_url = GURL("http://user1:pass2@malicious.url.com/");
    hit_report->page_url = GURL("http://page.url.com/");
    // Sanity check it is indeed the SanitizeHitReport method responsible for
    // the user1:pass2 being removed and not something about the URL being
    // invalid.
    EXPECT_EQ(hit_report->malicious_url.spec(),
              "http://user1:pass2@malicious.url.com/");
    ping_manager()->SanitizeHitReport(hit_report.get());
    EXPECT_EQ(hit_report->malicious_url.spec(), "http://malicious.url.com/");
    EXPECT_EQ(hit_report->page_url.spec(), "http://page.url.com/");
    EXPECT_EQ(hit_report->referrer_url.spec(), "");
  }
  // Multiple fields need sanitizing.
  {
    std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
    hit_report->malicious_url = GURL("http://user1:pass2@malicious.url.com/");
    hit_report->page_url = GURL("http://u1:p2@page.url.com/");
    hit_report->referrer_url = GURL("http://a:b@referrer.url.com/");
    ping_manager()->SanitizeHitReport(hit_report.get());
    EXPECT_EQ(hit_report->malicious_url.spec(), "http://malicious.url.com/");
    EXPECT_EQ(hit_report->page_url.spec(), "http://page.url.com/");
    EXPECT_EQ(hit_report->referrer_url.spec(), "http://referrer.url.com/");
  }
}

}  // namespace safe_browsing
