// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/build_time.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/version.h"
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

using certificate_transparency::LogInfo;
using net::NetLogWithSource;
using net::X509Certificate;
using net::ct::CTPolicyCompliance;
using net::ct::SCTList;
using net::ct::SignedCertificateTimestamp;

namespace certificate_transparency {

namespace {

// A log ID for test purposes.
const char kTestLogID[] =
    "\x68\xf6\x98\xf8\x1f\x64\x82\xbe\x3a\x8c\xee\xb9\x28\x1d\x4c\xfc\x71\x51"
    "\x5d\x67\x93\xd4\x44\xd1\x0a\x67\xac\xbb\x4f\x4f\xfb\xc4";
static_assert(std::size(kTestLogID) - 1 == crypto::kSHA256Length,
              "Incorrect log ID length.");

}  // namespace

class ChromeCTPolicyEnforcerTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_now_ = base::Time::Now();

    std::string der_test_cert(net::ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(
        base::as_bytes(base::make_span(der_test_cert)));
    ASSERT_TRUE(chain_.get());
    test_log_id_ = std::string(kTestLogID, crypto::kSHA256Length);
    another_log_id_.assign(crypto::kSHA256Length, 1);
  }

  scoped_refptr<ChromeCTPolicyEnforcer> MakeChromeCTPolicyEnforcer(
      std::vector<std::pair<std::string, base::Time>> disqualified_logs,
      std::map<std::string, LogInfo> log_info) {
    return base::MakeRefCounted<ChromeCTPolicyEnforcer>(
        test_now_, std::move(disqualified_logs), std::move(log_info),
        /*enable_static_ct_api_enforcement=*/true);
  }

  scoped_refptr<ChromeCTPolicyEnforcer> MakeChromeCTPolicyEnforcer(
      std::vector<std::pair<std::string, base::Time>> disqualified_logs,
      std::map<std::string, LogInfo> log_info,
      bool enable_static_ct_api_enforcement) {
    return base::MakeRefCounted<ChromeCTPolicyEnforcer>(
        test_now_, std::move(disqualified_logs), std::move(log_info),
        enable_static_ct_api_enforcement);
  }

  void FillListWithSCTsOfOrigin(
      SignedCertificateTimestamp::Origin desired_origin,
      size_t num_scts,
      const std::vector<std::string>& desired_log_keys,
      SCTList* verified_scts) {
    for (size_t i = 0; i < num_scts; ++i) {
      scoped_refptr<SignedCertificateTimestamp> sct(
          new SignedCertificateTimestamp());
      sct->origin = desired_origin;
      if (i < desired_log_keys.size())
        sct->log_id = desired_log_keys[i];
      else
        sct->log_id = std::string(crypto::kSHA256Length, static_cast<char>(i));

      EXPECT_TRUE(base::Time::FromUTCExploded({2022, 4, 0, 15, 0, 0, 0, 0},
                                              &sct->timestamp));

      verified_scts->push_back(sct);
    }
  }

  void AddDisqualifiedLogSCT(
      SignedCertificateTimestamp::Origin desired_origin,
      bool timestamp_after_disqualification_date,
      std::pair<std::string, base::Time>* disqualified_log,
      SCTList* verified_scts) {
    static const char kTestRetiredLogID[] =
        "\xcd\xb5\x17\x9b\x7f\xc1\xc0\x46\xfe\xea\x31\x13\x6a\x3f\x8f\x00\x2e"
        "\x61\x82\xfa\xf8\x89\x6f\xec\xc8\xb2\xf5\xb5\xab\x60\x49\x00";
    static_assert(std::size(kTestRetiredLogID) - 1 == crypto::kSHA256Length,
                  "Incorrect log ID length.");
    base::Time retirement_time;
    ASSERT_TRUE(base::Time::FromUTCExploded({2022, 4, 0, 16, 0, 0, 0, 0},
                                            &retirement_time));

    *disqualified_log =
        std::make_pair(std::string(kTestRetiredLogID, 32), retirement_time);

    scoped_refptr<SignedCertificateTimestamp> sct(
        new SignedCertificateTimestamp());
    sct->origin = desired_origin;
    sct->log_id = std::string(kTestRetiredLogID, crypto::kSHA256Length);
    if (timestamp_after_disqualification_date) {
      sct->timestamp = retirement_time + base::Hours(1);
    } else {
      sct->timestamp = retirement_time - base::Hours(1);
    }

    verified_scts->push_back(sct);
  }

  void FillListWithSCTsOfOrigin(
      SignedCertificateTimestamp::Origin desired_origin,
      size_t num_scts,
      SCTList* verified_scts) {
    std::vector<std::string> desired_log_ids;
    desired_log_ids.push_back(test_log_id_);
    FillListWithSCTsOfOrigin(desired_origin, num_scts, desired_log_ids,
                             verified_scts);
  }

  base::Time CreateTime(const base::Time::Exploded& exploded) {
    base::Time result;
    if (!base::Time::FromUTCExploded(exploded, &result)) {
      ADD_FAILURE() << "Failed FromUTCExploded";
    }
    return result;
  }

  void FillOperatorHistoryWithDiverseOperators(
      SCTList scts,
      std::map<std::string, LogInfo>* log_info) {
    for (size_t i = 0; i < scts.size(); i++) {
      OperatorHistoryEntry entry;
      entry.current_operator_ = "Operator " + base::NumberToString(i);
      LogInfo info;
      info.operator_history = entry;
      info.log_type = network::mojom::CTLogInfo::LogType::kRFC6962;
      (*log_info)[scts[i]->log_id] = info;
    }
  }

 protected:
  base::Time test_now_;
  scoped_refptr<X509Certificate> chain_;
  std::string test_log_id_;
  std::string another_log_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeCTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughFreshSCTs) {
  SCTList scts;
  std::pair<std::string, base::Time> disqualified_log;
  std::map<std::string, LogInfo> log_info;

  // The results should be the same before and after disqualification,
  // regardless of the delivery method.

  // Two SCTs from TLS, one of them from a disqualified log before the
  // disqualification time.
  scts.clear();
  log_info.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                        false, &disqualified_log, &scts);
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));

  // Two SCTs from TLS, one of them from a disqualified log after the
  // disqualification time.
  scts.clear();
  log_info.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                        true, &disqualified_log, &scts);
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  policy_enforcer = MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));

  // Two embedded SCTs, one of them from a disqualified log before the
  // disqualification time.
  scts.clear();
  log_info.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, false,
                        &disqualified_log, &scts);
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  policy_enforcer = MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));

  // Two embedded SCTs, one of them from a disqualified log after the
  // disqualification time.
  scts.clear();
  log_info.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, true,
                        &disqualified_log, &scts);
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  policy_enforcer = MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsToCTPolicyWithMixOfEmbeddedAndNonEmbedded) {
  SCTList scts;
  std::pair<std::string, base::Time> disqualified_log;
  std::map<std::string, LogInfo> log_info;

  // One SCT from TLS, one Embedded SCT from before disqualification time.
  scts.clear();
  log_info.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, false,
                        &disqualified_log, &scts);
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));

  // One SCT from TLS, one Embedded SCT from after disqualification time.
  // The embedded SCT is still counted towards the diversity requirement even
  // though it is disqualified.
  scts.clear();
  log_info.clear();
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           1, &scts);
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED, true,
                        &disqualified_log, &scts);
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  policy_enforcer = MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyWithNonEmbeddedSCTs) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, EnforcementDisabledByBinaryAge) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
            policy_enforcer->CheckCompliance(chain_.get(), scts,
                                             base::Time::Now() + base::Days(71),
                                             NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyWithEmbeddedSCTs) {
  // |chain_| is valid for 10 years - over 180 days - so requires 3 SCTs.
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 3, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsToCTPolicyWithPooledNonEmbeddedSCTs) {
  SCTList scts;
  std::vector<std::string> desired_logs;

  // One log, delivered via OCSP.
  desired_logs.clear();
  desired_logs.push_back(test_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE,
                           desired_logs.size(), desired_logs, &scts);

  // Another log, delivered via TLS.
  desired_logs.clear();
  desired_logs.push_back(another_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           desired_logs.size(), desired_logs, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyWithPooledEmbeddedSCTs) {
  SCTList scts;
  std::vector<std::string> desired_logs;

  // One log, delivered embedded.
  desired_logs.clear();
  desired_logs.push_back(test_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                           desired_logs.size(), desired_logs, &scts);

  // Another log, delivered via OCSP.
  desired_logs.clear();
  desired_logs.push_back(another_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE,
                           desired_logs.size(), desired_logs, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughSCTs) {
  // |chain_| is valid for 10 years - over 180 days - so requires 3 SCTs.
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 2, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsWithDisqualifiedLogBeforeDisqualificationDate) {
  SCTList scts;
  std::vector<std::string> desired_log_ids;
  desired_log_ids.push_back(test_log_id_);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                           desired_log_ids, &scts);
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                           std::vector<std::string>(), &scts);
  std::pair<std::string, base::Time> disqualified_log;
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED,
                        /*timestamp_after_disqualification_date=*/false,
                        &disqualified_log, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);

  // |chain_| is valid for 10 years - over 180 days - so requires 3 SCTs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformWithDisqualifiedLogAfterDisqualificationDate) {
  SCTList scts;
  // Add required - 1 valid SCTs.
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 2, &scts);
  std::pair<std::string, base::Time> disqualified_log;
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED,
                        /*timestamp_after_disqualification_date=*/true,
                        &disqualified_log, &scts);

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);

  // |chain_| is valid for 10 years - over 180 days - so requires 3 SCTs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformWithIssuanceDateAfterDisqualificationDate) {
  SCTList scts;
  std::pair<std::string, base::Time> disqualified_log;
  AddDisqualifiedLogSCT(SignedCertificateTimestamp::SCT_EMBEDDED,
                        /*timestamp_after_disqualification_date=*/true,
                        &disqualified_log, &scts);
  // Add required - 1 valid SCTs.
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 2, &scts);
  // Make sure all SCTs are after the disqualification date.
  for (size_t i = 1; i < scts.size(); ++i) {
    scts[i]->timestamp = scts[0]->timestamp;
  }

  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer({disqualified_log}, log_info);

  // |chain_| is valid for 10 years - over 180 days - so requires 3 SCTs.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, IsLogDisqualifiedTimestamp) {
  // Clear the log list and add 2 disqualified logs, one with a disqualification
  // date in the past and one in the future.
  const char kModifiedTestLogID[] =
      "\x68\xf6\x98\xf8\x1f\x64\x82\xbe\x3a\x8c\xee\xb9\x28\x1d\x4c\xfc\x71\x51"
      "\x5d\x67\x93\xd4\x44\xd1\x0a\x67\xac\xbb\x4f\x4f\x4f\xf4";
  std::vector<std::pair<std::string, base::Time>> disqualified_logs;
  std::map<std::string, LogInfo> log_info;
  base::Time now = base::Time::Now();
  base::Time past_disqualification = now - base::Hours(1);
  base::Time future_disqualification = now + base::Hours(1);
  disqualified_logs.emplace_back(kModifiedTestLogID, future_disqualification);
  disqualified_logs.emplace_back(kTestLogID, past_disqualification);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(disqualified_logs, log_info);

  base::Time disqualification_time;
  EXPECT_TRUE(policy_enforcer->IsLogDisqualified(kTestLogID, now,
                                                 &disqualification_time));
  EXPECT_EQ(disqualification_time, past_disqualification);
  EXPECT_EQ(policy_enforcer->GetLogDisqualificationTime(kTestLogID),
            past_disqualification);

  EXPECT_FALSE(policy_enforcer->IsLogDisqualified(kModifiedTestLogID, now,
                                                  &disqualification_time));
  EXPECT_EQ(disqualification_time, future_disqualification);
  EXPECT_EQ(policy_enforcer->GetLogDisqualificationTime(kModifiedTestLogID),
            future_disqualification);
}

TEST_F(ChromeCTPolicyEnforcerTest, IsLogDisqualifiedReturnsFalseOnUnknownLog) {
  // Clear the log list and add a single disqualified log, with a
  // disqualification date in the past;
  const char kModifiedTestLogID[] =
      "\x68\xf6\x98\xf8\x1f\x64\x82\xbe\x3a\x8c\xee\xb9\x28\x1d\x4c\xfc\x71\x51"
      "\x5d\x67\x93\xd4\x44\xd1\x0a\x67\xac\xbb\x4f\x4f\x4f\xf4";
  std::vector<std::pair<std::string, base::Time>> disqualified_logs;
  std::map<std::string, LogInfo> log_info;
  base::Time now = base::Time::Now();
  disqualified_logs.emplace_back(kModifiedTestLogID, now - base::Days(1));

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(disqualified_logs, log_info);

  base::Time unused;
  // IsLogDisqualified should return false for a log that is not in the
  // disqualified list.
  EXPECT_FALSE(policy_enforcer->IsLogDisqualified(kTestLogID, now, &unused));
  EXPECT_EQ(policy_enforcer->GetLogDisqualificationTime(kTestLogID),
            std::nullopt);
}

TEST_F(ChromeCTPolicyEnforcerTest,
       ConformsWithCTPolicyFutureRetirementDateLogs) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 5, &scts);

  std::vector<std::pair<std::string, base::Time>> disqualified_logs;
  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  // Set all the log operators for these SCTs as disqualified, with a timestamp
  // one hour from now.
  base::Time retirement_time = base::Time::Now() + base::Hours(1);
  // This mirrors how FillListWithSCTsOfOrigin generates log ids.
  disqualified_logs.emplace_back(test_log_id_, retirement_time);
  for (size_t i = 1; i < 5; ++i) {
    disqualified_logs.emplace_back(
        std::string(crypto::kSHA256Length, static_cast<char>(i)),
        retirement_time);
  }
  std::sort(std::begin(disqualified_logs), std::end(disqualified_logs));

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(disqualified_logs, log_info);

  // SCTs should comply since retirement date is in the future.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformWithCTPolicyPastRetirementDateLogs) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, 5, &scts);

  std::vector<std::pair<std::string, base::Time>> disqualified_logs;
  std::map<std::string, LogInfo> log_info;

  // Set all the log operators for these SCTs as disqualiied, with a timestamp
  // one hour ago.
  base::Time retirement_time = base::Time::Now() - base::Hours(1);
  // This mirrors how FillListWithSCTsOfOrigin generates log ids.
  disqualified_logs.emplace_back(test_log_id_, retirement_time);
  for (size_t i = 1; i < 5; ++i) {
    disqualified_logs.emplace_back(
        std::string(crypto::kSHA256Length, static_cast<char>(i)),
        retirement_time);
  }
  std::sort(std::begin(disqualified_logs), std::end(disqualified_logs));

  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(disqualified_logs, log_info);

  // SCTs should not comply since retirement date is in the past.
  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, UpdatedSCTRequirements) {
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

  base::Time time_2015_9_0_21_11_25_1_0 =
      CreateTime({2015, 9, 0, 21, 11, 25, 1, 0});

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
                   {// Cert valid for exactly 180 days, needs only 2 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2015_9_0_21_11_25_0_0, 2},
                   {// Cert valid for barely over 180 days, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2015_9_0_21_11_25_1_0, 3},
                   {// Cert valid for over 180 days, needs 3 SCTs.
                    time_2015_3_0_25_11_25_0_0, time_2016_3_0_25_11_25_0_0, 3}};

  for (size_t i = 0; i < std::size(kTestData); ++i) {
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

    std::map<std::string, LogInfo> log_info;
    for (size_t j = 0; j <= scts_required; ++j) {
      SCTList scts;
      FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED, j,
                               std::vector<std::string>(), &scts);
      // Add different operators to the logs so the SCTs comply with operator
      // diversity.
      FillOperatorHistoryWithDiverseOperators(scts, &log_info);

      scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
          MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

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
      EXPECT_EQ(expected,
                policy_enforcer->CheckCompliance(
                    cert.get(), scts, base::Time::Now(), NetLogWithSource()))
          << " for: " << (validity_end - validity_start).InDays() << " and "
          << scts_required << " scts=" << scts.size() << " j=" << j;
    }
  }
}

TEST_F(ChromeCTPolicyEnforcerTest,
       DoesNotConformToCTPolicyAllLogsSameOperator) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), &scts);
  std::map<std::string, LogInfo> log_info;
  for (auto sct : scts) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Operator";
    LogInfo info;
    info.operator_history = entry;
    info.log_type = network::mojom::CTLogInfo::LogType::kRFC6962;
    log_info[sct->log_id] = info;
  }

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToCTPolicyDifferentOperators) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), &scts);
  std::map<std::string, LogInfo> log_info;
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, ConformsToPolicyDueToOperatorSwitch) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), &scts);
  std::map<std::string, LogInfo> log_info;
  // Set all logs to the same operator.
  for (auto sct : scts) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Same Operator";
    LogInfo info;
    info.operator_history = entry;
    info.log_type = network::mojom::CTLogInfo::LogType::kRFC6962;
    log_info[sct->log_id] = info;
  }
  // Set the previous operator of one of the logs to a different one, with an
  // end time after the SCT timestamp.
  log_info[scts[1]->log_id].operator_history.previous_operators_.emplace_back(
      "Different Operator", scts[1]->timestamp + base::Seconds(1));

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, DoesNotConformToPolicyDueToOperatorSwitch) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), &scts);
  std::map<std::string, LogInfo> log_info;
  // Set logs to different operators
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);

  // Set the previous operator of one of the logs to the same as the other log,
  // with an end time after the SCT timestamp.
  log_info[scts[1]->log_id].operator_history.previous_operators_.emplace_back(
      "Operator 0", scts[1]->timestamp + base::Seconds(1));

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, MultipleOperatorSwitches) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), &scts);
  std::map<std::string, LogInfo> log_info;
  // Set logs to different operators
  FillOperatorHistoryWithDiverseOperators(scts, &log_info);
  // Set multiple previous operators, the first should be ignored since it
  // stopped operating before the SCT timestamp.
  log_info[scts[1]->log_id].operator_history.previous_operators_.emplace_back(
      "Different Operator", scts[1]->timestamp - base::Seconds(1));
  log_info[scts[1]->log_id].operator_history.previous_operators_.emplace_back(
      "Operator 0", scts[1]->timestamp + base::Seconds(1));

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, MultipleOperatorSwitchesBeforeSCTTimestamp) {
  SCTList scts;
  FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
                           2, std::vector<std::string>(), &scts);
  std::map<std::string, LogInfo> log_info;
  // Set all logs to the same operator.
  for (auto sct : scts) {
    OperatorHistoryEntry entry;
    entry.current_operator_ = "Same Operator";
    LogInfo info;
    info.operator_history = entry;
    info.log_type = network::mojom::CTLogInfo::LogType::kRFC6962;
    log_info[sct->log_id] = info;
  }
  // Set multiple previous operators, all of them should be ignored since they
  // all stopped operating before the SCT timestamp.
  log_info[scts[1]->log_id].operator_history.previous_operators_.emplace_back(
      "Different Operator", scts[1]->timestamp - base::Seconds(2));
  log_info[scts[1]->log_id].operator_history.previous_operators_.emplace_back(
      "Yet Another Different Operator", scts[1]->timestamp - base::Seconds(1));

  scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
      MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info);

  EXPECT_EQ(CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            policy_enforcer->CheckCompliance(
                chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
}

TEST_F(ChromeCTPolicyEnforcerTest, DoesNotConformToCTPolicyNoRFC6962Log) {
  struct TestCase {
    const char* const name;
    bool enable_static_ct_api_enforcement;
    size_t sct_count;
    CTPolicyCompliance result;
  } kTestCases[] = {
      {"Not enough SCTs with StaticCT API Policy disabled",
       /*enable_static_ct_api_enforcement=*/false, 2,
       CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS},
      {"Enough SCTs with StaticCT API Policy disabled",
       /*enable_static_ct_api_enforcement=*/false, 3,
       CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS},
      {"Not enough SCTs with Static CT API Policy enabled",
       /*enable_static_ct_api_enforcement=*/true, 2,
       // TODO(crbug.com/370724580): Reconsider this, might also return
       // CT_POLICY_NOT_ENOUGH_SCTS.
       CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS},
      {"Enough SCTs with Static CT API Policy Enabled",
       /*enable_static_ct_api_enforcement=*/true, 3,
       CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS},
  };

  for (const TestCase& tc : kTestCases) {
    SCOPED_TRACE(tc.name);
    SCTList scts;
    FillListWithSCTsOfOrigin(SignedCertificateTimestamp::SCT_EMBEDDED,
                             tc.sct_count, &scts);

    std::map<std::string, LogInfo> log_info;
    FillOperatorHistoryWithDiverseOperators(scts, &log_info);
    // Set all logs to a non-RFC6962 log type.
    for (size_t i = 0; i < scts.size(); i++) {
      log_info[scts[i]->log_id].log_type =
          network::mojom::CTLogInfo::LogType::kStaticCTAPI;
    }

    scoped_refptr<ChromeCTPolicyEnforcer> policy_enforcer =
        MakeChromeCTPolicyEnforcer(GetDisqualifiedLogs(), log_info,
                                   /*enable_static_ct_api_enforcement=*/
                                   tc.enable_static_ct_api_enforcement);

    EXPECT_EQ(tc.result,
              policy_enforcer->CheckCompliance(
                  chain_.get(), scts, base::Time::Now(), NetLogWithSource()));
  }
}

}  // namespace certificate_transparency
