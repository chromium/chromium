// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"

#include <memory>
#include <string>

#include "base/build_time.h"
#include "base/stl_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_verify_result.h"
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
        base::GetBuildTime(), GetDisqualifiedLogs(), GetLogsOperatedByGoogle());
    enforcer->SetClockForTesting(&clock_);
    policy_enforcer_.reset(enforcer.release());

    std::string der_test_cert(net::ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(der_test_cert.data(),
                                              der_test_cert.size());
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

  clock_.Advance(base::TimeDelta::FromDays(71));

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
    scoped_refptr<X509Certificate> cert(
        X509Certificate::CreateFromBytes(cert_data.data(), cert_data.size()));
    ASSERT_TRUE(cert);

    for (size_t i = 0; i < required_scts - 1; ++i) {
      SCTList scts;
      FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, i,
                               std::vector<std::string>(), false, &scts);
      EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                policy_enforcer_->CheckCompliance(cert.get(), scts,
                                                  NetLogWithSource()))
          << " for: " << (end - start).InDays() << " and " << required_scts
          << " scts=" << scts.size() << " i=" << i;
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

}  // namespace

}  // namespace certificate_transparency
