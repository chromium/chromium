// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/build_time.h"
#include "base/cxx17_backports.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/certificate_transparency/ct_features.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::ct::CTPolicyCompliance;
using net::ct::SCTList;
using net::ct::SignedCertificateTimestamp;
using net::NetLogWithSource;
using net::X509Certificate;

namespace certificate_transparency {

namespace {

const char kGoogleAviatorLogID[] =
    "\x68\xf6\x98\xf8\x1f\x64\x82\xbe\x3a\x8c\xee\xb9\x28\x1d\x4c\xfc\x71\x51"
    "\x5d\x67\x93\xd4\x44\xd1\x0a\x67\xac\xbb\x4f\x4f\xfb\xc4";
static_assert(base::size(kGoogleAviatorLogID) - 1 == crypto::kSHA256Length,
              "Incorrect log ID length.");

class ChromeCTPolicyEnforcerTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto enforcer = std::make_unique<ChromeCTPolicyEnforcer>(
        base::Time::Now(), GetDisqualifiedLogs(), GetLogsOperatedByGoogle(),
        std::map<std::string, OperatorHistoryEntry>());
    enforcer->SetClockForTesting(&clock_);
    policy_enforcer_ = std::move(enforcer);

    std::string der_test_cert(net::ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(
        base::as_bytes(base::make_span(der_test_cert)));
    ASSERT_TRUE(chain_.get());
    google_log_id_ = std::string(kGoogleAviatorLogID, crypto::kSHA256Length);
    non_google_log_id_.assign(crypto::kSHA256Length, 1);
    clock_.SetNow(base::Time::Now());
  }

  void FillListWithSCTsOfOrigin(
      SignedCertificateTimestamp::Origin desired_origin,
      size_t num_scts,
      const std::vector<std::string>& desired_log_keys,
      bool timestamp_past_enforcement_date,
      SCTList* verified_scts) {
    for (size_t i = 0; i < num_scts; ++i) {
      scoped_refptr<SignedCertificateTimestamp> sct(
          new SignedCertificateTimestamp());
      sct->origin = desired_origin;
      if (i < desired_log_keys.size())
        sct->log_id = desired_log_keys[i];
      else
        sct->log_id = std::string(crypto::kSHA256Length, static_cast<char>(i));

      if (timestamp_past_enforcement_date) {
        EXPECT_TRUE(base::Time::FromUTCExploded({2015, 8, 0, 15, 0, 0, 0, 0},
                                                &sct->timestamp));
      } else {
        EXPECT_TRUE(base::Time::FromUTCExploded({2015, 6, 0, 15, 0, 0, 0, 0},
                                                &sct->timestamp));
      }

      verified_scts->push_back(sct);
    }
  }

  void AddDisqualifiedLogSCT(SignedCertificateTimestamp::Origin desired_origin,
                             bool timestamp_after_disqualification_date,
                             SCTList* verified_scts) {
    static const char kCertlyLogID[] =
        "\xcd\xb5\x17\x9b\x7f\xc1\xc0\x46\xfe\xea\x31\x13\x6a\x3f\x8f\x00\x2e"
        "\x61\x82\xfa\xf8\x89\x6f\xec\xc8\xb2\xf5\xb5\xab\x60\x49\x00";
    static_assert(base::size(kCertlyLogID) - 1 == crypto::kSHA256Length,
                  "Incorrect log ID length.");

    scoped_refptr<SignedCertificateTimestamp> sct(
        new SignedCertificateTimestamp());
    sct->origin = desired_origin;
    sct->log_id = std::string(kCertlyLogID, crypto::kSHA256Length);
    if (timestamp_after_disqualification_date) {
      EXPECT_TRUE(base::Time::FromUTCExploded({2016, 4, 0, 16, 0, 0, 0, 0},
                                              &sct->timestamp));
    } else {
      EXPECT_TRUE(base::Time::FromUTCExploded({2016, 4, 0, 1, 0, 0, 0, 0},
                                              &sct->timestamp));
    }

    verified_scts->push_back(sct);
  }

  void FillListWithSCTsOfOrigin(
      SignedCertificateTimestamp::Origin desired_origin,
      size_t num_scts,
      SCTList* verified_scts) {
    std::vector<std::string> desired_log_ids;
    desired_log_ids.push_back(google_log_id_);
    FillListWithSCTsOfOrigin(desired_origin, num_scts, desired_log_ids, true,
                             verified_scts);
  }

  base::Time CreateTime(const base::Time::Exploded& exploded) {
    base::Time result;
    if (!base::Time::FromUTCExploded(exploded, &result)) {
      ADD_FAILURE() << "Failed FromUTCExploded";
    }
    return result;
  }

 protected:
  base::SimpleTestClock clock_;
  std::unique_ptr<net::CTPolicyEnforcer> policy_enforcer_;
  scoped_refptr<X509Certificate> chain_;
  std::string google_log_id_;
  std::string non_google_log_id_;
};

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformToCTPolicyNotEnoughDiverseSCTsAllGoogle) {
  SCTList scts;
  std::vector<std::string> desired_log_ids(2, google_log_id_);

  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           desired_log_ids.size(), desired_log_ids, true,
                           &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformToCTPolicyNotEnoughDiverseSCTsAllNonGoogle) {
  SCTList scts;
  std::vector<std::string> desired_log_ids(2, non_google_log_id_);

  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           desired_log_ids.size(), desired_log_ids, true,
                           &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsToCTPolicyIfSCTBeforeEnforcementDate) {
  SCTList scts;
  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  // All 5 SCTs will be from non-Google logs.
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 5,
                           std::vector<std::string>(), false, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyWithNonEmbeddedSCTs) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, EnforcementDisabledByBinaryAge) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));

  clock_.Advance(base::Days(71));

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyWithEmbeddedSCTs) {
  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 5, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsToCTPolicyWithPooledNonEmbeddedSCTs) {
  SCTList scts;
  std::vector<std::string> desired_logs;

  // One Google log, delivered via OCSP.
  desired_logs.clear();
  desired_logs.push_back(google_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE,
                           desired_logs.size(), desired_logs, true, &scts);

  // One non-Google log, delivered via TLS.
  desired_logs.clear();
  desired_logs.push_back(non_google_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           desired_logs.size(), desired_logs, true, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyWithPooledEmbeddedSCTs) {
  SCTList scts;
  std::vector<std::string> desired_logs;

  // One Google log, delivered embedded.
  desired_logs.clear();
  desired_logs.push_back(google_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                           desired_logs.size(), desired_logs, true, &scts);

  // One non-Google log, delivered via OCSP.
  desired_logs.clear();
  desired_logs.push_back(non_google_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE,
                           desired_logs.size(), desired_logs, true, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughSCTs) {
  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 2, &scts);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughFreshSCTs) {
  SCTList scts;

  // The results should be the same before and after disqualification,
  // regardless of the delivery method.

  // SCT from before disqualification.
  scts.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                        false, &scts);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));

  // SCT from after disqualification.
  scts.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                        true, &scts);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));

  // Embedded SCT from before disqualification.
  scts.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, false, &scts);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));

  // Embedded SCT from after disqualification.
  scts.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, true, &scts);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsWithDisqualifiedLogBeforeDisqualificationDate) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 4, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, false, &scts);

  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformWithDisqualifiedLogAfterDisqualificationDate) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 4, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, true, &scts);

  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformWithIssuanceDateAfterDisqualificationDate) {
  SCTList scts;
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, true, &scts);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 4, &scts);
  // Make sure all SCTs are after the disqualification date.
  for (size_t i = 1; i < scts.size(); ++i)
    scts[i]->timestamp = scts[0]->timestamp;

  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformToCTPolicyNotEnoughUniqueEmbeddedLogs) {
  SCTList scts;
  std::vector<std::string> desired_logs;

  // One Google Log.
  desired_logs.clear();
  desired_logs.push_back(google_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                           desired_logs.size(), desired_logs, true, &scts);

  // Two distinct non-Google logs.
  desired_logs.clear();
  desired_logs.push_back(std::string(crypto::kSHA256Length, 'A'));
  desired_logs.push_back(std::string(crypto::kSHA256Length, 'B'));
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                           desired_logs.size(), desired_logs, true, &scts);

  // Two unique SCTs from the same non-Google log.
  desired_logs.clear();
  desired_logs.push_back(std::string(crypto::kSHA256Length, 'C'));
  desired_logs.push_back(std::string(crypto::kSHA256Length, 'C'));
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                           desired_logs.size(), desired_logs, true, &scts);

  // |chain_| is valid for 10 years - over 121 months - so requires 5 SCTs.
  // However, there are only 4 SCTs are from distinct logs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsToPolicyExactNumberOfSCTsForValidityPeriod) {
  std::unique_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(private_key);

  // Test multiple validity periods
  base::Time time_2015_3_0_25_11_25_0_0 =
      CreateTime({2015, 3, 0, 25, 11, 25, 0, 0});

  base::Time time_2016_6_0_6_11_25_0_0 =
      CreateTime({2016, 6, 0, 6, 11, 25, 0, 0});

  base::Time time_2016_6_0_25_11_25_0_0 =
      CreateTime({2016, 6, 0, 25, 11, 25, 0, 0});

  base::Time time_2016_6_0_27_11_25_0_0 =
      CreateTime({2016, 6, 0, 27, 11, 25, 0, 0});

  base::Time time_2017_6_0_25_11_25_0_0 =
      CreateTime({2017, 6, 0, 25, 11, 25, 0, 0});

  base::Time time_2017_6_0_28_11_25_0_0 =
      CreateTime({2017, 6, 0, 28, 11, 25, 0, 0});

  base::Time time_2018_6_0_25_11_25_0_0 =
      CreateTime({2018, 6, 0, 25, 11, 25, 0, 0});

  base::Time time_2018_6_0_27_11_25_0_0 =
      CreateTime({2018, 6, 0, 27, 11, 25, 0, 0});

  const struct TestData {
    base::Time validity_start;
    base::Time validity_end;
    size_t scts_required;
  } kTestData[] = {{// Cert valid for -14 months (nonsensical), needs 2 SCTs.
                    time_2016_6_0_6_11_25_0_0, time_2015_3_0_25_11_25_0_0, 2},
                   {// Cert valid for 14 months, needs 2 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2016_6_0_6_11_25_0_0, 2},
                   {// Cert valid for exactly 15 months, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2016_6_0_25_11_25_0_0, 3},
                   {// Cert valid for over 15 months, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2016_6_0_27_11_25_0_0, 3},
                   {// Cert valid for exactly 27 months, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2017_6_0_25_11_25_0_0, 3},
                   {// Cert valid for over 27 months, needs 4 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2017_6_0_28_11_25_0_0, 4},
                   {// Cert valid for exactly 39 months, needs 4 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2018_6_0_25_11_25_0_0, 4},
                   {// Cert valid for over 39 months, needs 5 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2018_6_0_27_11_25_0_0, 5}};

  for (size_t i = 0; i < base::size(kTestData); ++i) {
    SCOPED_TRACE(i);
    const base::Time& start = kTestData[i].validity_start;
    const base::Time& end = kTestData[i].validity_end;
    size_t required_scts = kTestData[i].scts_required;

    // Create a self-signed certificate with exactly the validity period.
    std::string cert_data;
    ASSERT_TRUE(net::x509_util::CreateSelfSignedCert(
        private_key->key(), net::x509_util::DIGEST_SHA256, "CN=test",
        i * 10 + required_scts, start, end, {}, &cert_data));
    scoped_refptr<X509Certificate> cert(X509Certificate::CreateFromBytes(
        base::as_bytes(base::make_span(cert_data))));
    ASSERT_TRUE(cert);

    for (size_t j = 0; j < required_scts - 1; ++j) {
      SCTList scts;
      FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, j,
                               std::vector<std::string>(), false, &scts);
      EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                policy_enforcer_->CheckCompliance(cert.get(), scts,
                                                  NetLogWithSource()))
          << " for: " << (end - start).InDays() << " and " << required_scts
          << " scts=" << scts.size() << " j=" << j;
    }
    SCTList scts;
    FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                             required_scts, std::vector<std::string>(), false,
                             &scts);
    EXPECT_EQ(
        CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
        policy_enforcer_->CheckCompliance(cert.get(), scts, NetLogWithSource()))
        << " for: " << (end - start).InDays() << " and " << required_scts
        << " scts=" << scts.size();
  }
}

TEST_F(ChromeCTPolicyEnforcerTest, UpdateCTLogList) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, &scts);

  std::vector<std::pair<std::string, base::TimeDelta>> disqualified_logs;
  std::vector<std::string> operated_by_google_logs;
  chrome_policy_enforcer->UpdateCTLogList(base::Time::Now(), disqualified_logs,
                                          operated_by_google_logs,
                                          /*log_operator_history=*/{});

  // The check should fail since the Google Aviator log is no longer in the
  // list after the update with an empty list.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            chrome_policy_enforcer->CheckCompliance(chain_.get(), scts,
                                                    NetLogWithSource()));

  // Update the list again, this time including all the known operated by Google
  // logs.
  operated_by_google_logs = certificate_transparency::GetLogsOperatedByGoogle();
  chrome_policy_enforcer->UpdateCTLogList(base::Time::Now(), disqualified_logs,
                                          operated_by_google_logs,
                                          /*log_operator_history=*/{});

  // The check should now succeed.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            chrome_policy_enforcer->CheckCompliance(chain_.get(), scts,
                                                    NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, TimestampUpdates) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, &scts);

  // Clear the log list and set the last updated time to more than 10 weeks ago.
  std::vector<std::pair<std::string, base::TimeDelta>> disqualified_logs;
  std::vector<std::string> operated_by_google_logs;
  std::map<std::string, OperatorHistoryEntry> log_operator_history;
  chrome_policy_enforcer->UpdateCTLogList(
      base::Time::Now() - base::Days(71), disqualified_logs,
      operated_by_google_logs, log_operator_history);

  // The check should return build not timely even though the Google Aviator log
  // is no longer in the list, since the last update time is greater than 10
  // weeks.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
            chrome_policy_enforcer->CheckCompliance(chain_.get(), scts,
                                                    NetLogWithSource()));

  // Update the last update time value again, this time with a recent time.
  chrome_policy_enforcer->UpdateCTLogList(base::Time::Now(), disqualified_logs,
                                          operated_by_google_logs,
                                          log_operator_history);

  // The check should now fail
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            chrome_policy_enforcer->CheckCompliance(chain_.get(), scts,
                                                    NetLogWithSource()));
}

class ChromeCTPolicyEnforcerTest2022Policy : public ChromeCTPolicyEnforcerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCertificateTransparency2022Policy);
    ChromeCTPolicyEnforcerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeCTPolicyEnforcerTest2022Policy,
       2022PolicyNotInEffectBeforeTargetDate) {
  // Old policy should enforce one Google log requirement.
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  for (size_t i = 0; i < scts.size(); i++) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator " + base::NumberToString(i);
    operator_history[scts[i]->log_id] = entry;
    // Set timestamp to 1 day before new policy comes in effect.
    EXPECT_TRUE(base::Time::FromUTCExploded({2022, 1, 0, 31, 0, 0, 0, 0},
                                            &scts[i]->timestamp));
  }
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest2022Policy,
       2022PolicyInEffectAfterTargetDate) {
  // New policy should allow SCTs from all non-Google operators to comply as
  // long as diversity requirement is fulfilled.
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  for (size_t i = 0; i < scts.size(); i++) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator " + base::NumberToString(i);
    operator_history[scts[i]->log_id] = entry;
    // Set timestamp to the day new policy comes in effect.
    EXPECT_TRUE(base::Time::FromUTCExploded({2022, 2, 0, 1, 0, 0, 0, 0},
                                            &scts[i]->timestamp));
  }
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

class ChromeCTPolicyEnforcerTest2022PolicyAllCerts
    : public ChromeCTPolicyEnforcerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCertificateTransparency2022PolicyAllCerts);
    ChromeCTPolicyEnforcerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts, UpdatedSCTRequirements) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());

  std::unique_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(private_key);

  // Test multiple validity periods
  base::Time time_2015_3_0_25_11_25_0_0 =
      CreateTime({2015, 3, 0, 25, 11, 25, 0, 0});

  base::Time time_2015_9_0_20_11_25_0_0 =
      CreateTime({2015, 9, 0, 20, 11, 25, 0, 0});

  base::Time time_2015_9_0_21_11_25_0_0 =
      CreateTime({2015, 9, 0, 21, 11, 25, 0, 0});

  base::Time time_2016_3_0_25_11_25_0_0 =
      CreateTime({2016, 3, 0, 25, 11, 25, 0, 0});

  const struct TestData {
    base::Time validity_start;
    base::Time validity_end;
    size_t scts_required;
  } kTestData[] = {{// Cert valid for -12 months (nonsensical), needs 2 SCTs.
                    time_2016_3_0_25_11_25_0_0, time_2015_3_0_25_11_25_0_0, 2},
                   {// Cert valid for 179 days, needs 2 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2015_9_0_20_11_25_0_0, 2},
                   {// Cert valid for exactly 180 days, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2015_9_0_21_11_25_0_0, 3},
                   {// Cert valid for over 180 days, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2016_3_0_25_11_25_0_0, 3}};

  for (size_t i = 0; i < base::size(kTestData); ++i) {
    SCOPED_TRACE(i);
    const base::Time& validity_start = kTestData[i].validity_start;
    const base::Time& validity_end = kTestData[i].validity_end;
    size_t scts_required = kTestData[i].scts_required;

    // Create a self-signed certificate with exactly the validity period.
    std::string cert_data;
    ASSERT_TRUE(net::x509_util::CreateSelfSignedCert(
        private_key->key(), net::x509_util::DIGEST_SHA256, "CN=test",
        i * 10 + scts_required, validity_start, validity_end, {}, &cert_data));
    scoped_refptr<X509Certificate> cert(X509Certificate::CreateFromBytes(
        base::as_bytes(base::make_span(cert_data))));
    ASSERT_TRUE(cert);

    std::map<std::string, OperatorHistoryEntry> operator_history;
    for (size_t j = 0; j <= scts_required - 1; ++j) {
      SCTList scts;
      FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, j,
                               std::vector<std::string>(), false, &scts);
      // Add different operators to the logs so the SCTs comply with operator
      // diversity.
      for (size_t k = 0; k < scts.size(); k++) {
        OperatorHistoryEntry entry;
        entry.current_operator_ = "Operator " + base::NumberToString(k);
        operator_history[scts[k]->log_id] = entry;
      }
      chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);
      CTPolicyCompliance expected;
      if (j == scts_required) {
        // If the scts provided are as many as are required, the cert should be
        // declared as compliant.
        expected = CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
      } else if (j == 1) {
        // If a single SCT is provided, it should trip the diversity check.
        expected = CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
      } else {
        // In any other case, the 'not enough' check should trip.
        expected = CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
      }
      EXPECT_EQ(expected, policy_enforcer_->CheckCompliance(cert.get(), scts,
                                                            NetLogWithSource()))
          << " for: " << (validity_end - validity_start).InDays() << " and "
          << scts_required << " scts=" << scts.size() << " j=" << j;
    }
  }
}

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts,
       DoesNotConformToCTPolicyAllLogsSameOperator) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  for (size_t i = 0; i < scts.size(); i++) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator";
    operator_history[scts[i]->log_id] = entry;
  }
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts,
       ConformsToCTPolicyDifferentOperators) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  for (size_t i = 0; i < scts.size(); i++) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator " + base::NumberToString(i);
    operator_history[scts[i]->log_id] = entry;
  }
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts,
       ConformsToPolicyDueToOperatorSwitch) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  // Set all logs to the same operator.
  for (auto sct : scts) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Same Operator";
    operator_history[sct->log_id] = entry;
  }
  // Set the previous operator of one of the logs to a different one, with an
  // end time after the SCT timestamp.
  operator_history[scts[1]->log_id].previous_operators_.emplace_back(
      "Different Operator",
      scts[1]->timestamp + base::Seconds(1) - base::Time::UnixEpoch());
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts,
       DoesNotConformToPolicyDueToOperatorSwitch) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  // Set logs to different operators
  for (size_t i = 0; i < scts.size(); i++) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator " + base::NumberToString(i);
    operator_history[scts[i]->log_id] = entry;
  }
  // Set the previous operator of one of the logs to the same as the other log,
  // with an end time after the SCT timestamp.
  operator_history[scts[1]->log_id].previous_operators_.emplace_back(
      "Operator 0",
      scts[1]->timestamp + base::Seconds(1) - base::Time::UnixEpoch());
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts, MultipleOperatorSwitches) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  // Set logs to different operators
  for (size_t i = 0; i < scts.size(); i++) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator " + base::NumberToString(i);
    operator_history[scts[i]->log_id] = entry;
  }
  // Set multiple previous operators, the first should be ignored since it
  // stopped operating before the SCT timestamp.
  operator_history[scts[1]->log_id].previous_operators_.emplace_back(
      "Different Operator",
      scts[1]->timestamp - base::Seconds(1) - base::Time::UnixEpoch());
  operator_history[scts[1]->log_id].previous_operators_.emplace_back(
      "Operator 0",
      scts[1]->timestamp + base::Seconds(1) - base::Time::UnixEpoch());
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest2022PolicyAllCerts,
       MultipleOperatorSwitchesBeforeSCTTimestamp) {
  ChromeCTPolicyEnforcer* chrome_policy_enforcer =
      static_cast<ChromeCTPolicyEnforcer*>(policy_enforcer_.get());
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), true, &scts);
  std::map<std::string, OperatorHistoryEntry> operator_history;
  // Set all logs to the same operator.
  for (auto sct : scts) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Same Operator";
    operator_history[sct->log_id] = entry;
  }
  // Set multiple previous operators, all of them should be ignored since they
  // all stopped operating before the SCT timestamp.
  operator_history[scts[1]->log_id].previous_operators_.emplace_back(
      "Different Operator",
      scts[1]->timestamp - base::Seconds(2) - base::Time::UnixEpoch());
  operator_history[scts[1]->log_id].previous_operators_.emplace_back(
      "Yet Another Different Operator",
      scts[1]->timestamp - base::Seconds(1) - base::Time::UnixEpoch());
  chrome_policy_enforcer->SetOperatorHistoryForTesting(operator_history);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer_->CheckCompliance(chain_.get(), scts,
                                              NetLogWithSource()));
}

}  // namespace

}  // namespace certificate_transparency
