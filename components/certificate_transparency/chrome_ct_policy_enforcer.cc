// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "crypto/sha2.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

using net::ct::CTPolicyCompliance;

namespace certificate_transparency {

namespace {

// Returns a rounded-down months difference of |start| and |end|,
// together with an indication of whether the last month was
// a full month, because the range starts specified in the policy
// are not consistent in terms of including the range start value.
void RoundedDownMonthDifference(const base::Time& start,
                                const base::Time& end,
                                size_t* rounded_months_difference,
                                bool* has_partial_month) {
  DCHECK(rounded_months_difference);
  DCHECK(has_partial_month);
  base::Time::Exploded exploded_start;
  base::Time::Exploded exploded_expiry;
  start.UTCExplode(&exploded_start);
  end.UTCExplode(&exploded_expiry);
  if (end < start) {
    *rounded_months_difference = 0;
    *has_partial_month = false;
    return;
  }

  *has_partial_month = true;
  uint32_t month_diff = (exploded_expiry.year - exploded_start.year) * 12 +
                        (exploded_expiry.month - exploded_start.month);
  if (exploded_expiry.day_of_month < exploded_start.day_of_month)
    --month_diff;
  else if (exploded_expiry.day_of_month == exploded_start.day_of_month)
    *has_partial_month = false;

  *rounded_months_difference = month_diff;
}

const char* CTPolicyComplianceToString(CTPolicyCompliance status) {
  switch (status) {
    case CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS:
      return "COMPLIES_VIA_SCTS";
    case CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS:
      return "NOT_ENOUGH_SCTS";
    case CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS:
      return "NOT_DIVERSE_SCTS";
    case CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY:
      return "BUILD_NOT_TIMELY";
    case CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE:
    case CTPolicyCompliance::CT_POLICY_COUNT:
      NOTREACHED();
      return "unknown";
  }

  NOTREACHED();
  return "unknown";
}

base::Value NetLogCertComplianceCheckResultParams(
    net::X509Certificate* cert,
    bool build_timely,
    CTPolicyCompliance compliance) {
  base::DictionaryValue dict;
  dict.SetKey("certificate", net::NetLogX509CertificateParams(cert));
  dict.SetBoolean("build_timely", build_timely);
  dict.SetString("ct_compliance_status",
                 CTPolicyComplianceToString(compliance));
  return std::move(dict);
}

}  // namespace

ChromeCTPolicyEnforcer::ChromeCTPolicyEnforcer(
    base::Time log_list_date,
    std::vector<std::pair<std::string, base::TimeDelta>> disqualified_logs,
    std::vector<std::string> operated_by_google_logs)
    : disqualified_logs_(disqualified_logs),
      operated_by_google_logs_(operated_by_google_logs),
      clock_(base::DefaultClock::GetInstance()),
      log_list_date_(log_list_date) {}

ChromeCTPolicyEnforcer::~ChromeCTPolicyEnforcer() {}

CTPolicyCompliance ChromeCTPolicyEnforcer::CheckCompliance(
    net::X509Certificate* cert,
    const net::ct::SCTList& verified_scts,
    const net::NetLogWithSource& net_log) {
  // If the build is not timely, no certificate is considered compliant
  // with CT policy. The reasoning is that, for example, a log might
  // have been pulled and is no longer considered valid; thus, a client
  // needs up-to-date information about logs to consider certificates to
  // be compliant with policy.
  bool build_timely = IsLogDataTimely();
  CTPolicyCompliance compliance;
  if (!build_timely) {
    compliance = CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY;
  } else {
    compliance = CheckCTPolicyCompliance(*cert, verified_scts);
  }

  net_log.AddEvent(net::NetLogEventType::CERT_CT_COMPLIANCE_CHECKED, [&] {
    return NetLogCertComplianceCheckResultParams(cert, build_timely,
                                                 compliance);
  });

  return compliance;
}

bool ChromeCTPolicyEnforcer::IsLogDisqualified(
    base::StringPiece log_id,
    base::Time* disqualification_date) const {
  CHECK_EQ(log_id.size(), crypto::kSHA256Length);

  auto p = std::lower_bound(
      std::begin(disqualified_logs_), std::end(disqualified_logs_), log_id,
      [](const auto& a, base::StringPiece b) { return a.first < b; });
  if (p == std::end(disqualified_logs_) || p->first != log_id) {
    return false;
  }
  *disqualification_date = base::Time::UnixEpoch() + p->second;
  return true;
}

bool ChromeCTPolicyEnforcer::IsLogOperatedByGoogle(
    base::StringPiece log_id) const {
  return std::binary_search(std::begin(operated_by_google_logs_),
                            std::end(operated_by_google_logs_), log_id);
}

bool ChromeCTPolicyEnforcer::IsLogDataTimely() const {
  // We consider built-in information to be timely for 10 weeks.
  return (clock_->Now() - log_list_date_).InDays() < 70 /* 10 weeks */;
}

// Evaluates against the policy specified at
// https://sites.google.com/a/chromium.org/dev/Home/chromium-security/root-ca-policy/EVCTPlanMay2015edition.pdf?attredirects=0
CTPolicyCompliance ChromeCTPolicyEnforcer::CheckCTPolicyCompliance(
    const net::X509Certificate& cert,
    const net::ct::SCTList& verified_scts) const {
  // Cert is outside the bounds of parsable; reject it.
  if (cert.valid_start().is_null() || cert.valid_expiry().is_null() ||
      cert.valid_start().is_max() || cert.valid_expiry().is_max()) {
    return CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  }

  // Scan for the earliest SCT. This is used to determine whether to enforce
  // log diversity requirements, as well as whether to enforce whether or not
  // a log was qualified or pending qualification at time of issuance (in the
  // case of embedded SCTs). It's acceptable to ignore the origin of the SCT,
  // because SCTs delivered via OCSP/TLS extension will cover the full
  // certificate, which necessarily will exist only after the precertificate
  // has been logged and the actual certificate issued.
  // Note: Here, issuance date is defined as the earliest of all SCTs, rather
  // than the latest of embedded SCTs, in order to give CAs the benefit of
  // the doubt in the event a log is revoked in the midst of processing
  // a precertificate and issuing the certificate.
  base::Time issuance_date = base::Time::Max();
  for (const auto& sct : verified_scts) {
    base::Time unused;
    if (IsLogDisqualified(sct->log_id, &unused))
      continue;
    issuance_date = std::min(sct->timestamp, issuance_date);
  }

  bool has_valid_google_sct = false;
  bool has_valid_nongoogle_sct = false;
  bool has_valid_embedded_sct = false;
  bool has_valid_nonembedded_sct = false;
  bool has_embedded_google_sct = false;
  bool has_embedded_nongoogle_sct = false;
  std::vector<base::StringPiece> embedded_log_ids;
  for (const auto& sct : verified_scts) {
    base::Time disqualification_date;
    bool is_disqualified =
        IsLogDisqualified(sct->log_id, &disqualification_date);
    if (is_disqualified &&
        sct->origin != net::ct::SignedCertificateTimestamp::SCT_EMBEDDED) {
      // For OCSP and TLS delivered SCTs, only SCTs that are valid at the
      // time of check are accepted.
      continue;
    }

    if (IsLogOperatedByGoogle(sct->log_id)) {
      has_valid_google_sct |= !is_disqualified;
      if (sct->origin == net::ct::SignedCertificateTimestamp::SCT_EMBEDDED)
        has_embedded_google_sct = true;
    } else {
      has_valid_nongoogle_sct |= !is_disqualified;
      if (sct->origin == net::ct::SignedCertificateTimestamp::SCT_EMBEDDED)
        has_embedded_nongoogle_sct = true;
    }
    if (sct->origin != net::ct::SignedCertificateTimestamp::SCT_EMBEDDED) {
      has_valid_nonembedded_sct = true;
    } else {
      has_valid_embedded_sct |= !is_disqualified;
      // If the log is disqualified, it only counts towards quorum if
      // the certificate was issued before the log was disqualified, and the
      // SCT was obtained before the log was disqualified.
      if (!is_disqualified || (issuance_date < disqualification_date &&
                               sct->timestamp < disqualification_date)) {
        embedded_log_ids.push_back(sct->log_id);
      }
    }
  }

  // Option 1:
  // An SCT presented via the TLS extension OR embedded within a stapled OCSP
  //   response is from a log qualified at time of check;
  // AND there is at least one SCT from a Google Log that is qualified at
  //   time of check, presented via any method;
  // AND there is at least one SCT from a non-Google Log that is qualified
  //   at the time of check, presented via any method.
  //
  // Note: Because SCTs embedded via TLS or OCSP can be updated on the fly,
  // the issuance date is irrelevant, as any policy changes can be
  // accomodated.
  if (has_valid_nonembedded_sct && has_valid_google_sct &&
      has_valid_nongoogle_sct) {
    return CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  }
  // Note: If has_valid_nonembedded_sct was true, but Option 2 isn't met,
  // then the result will be that there weren't diverse enough SCTs, as that
  // the only other way for the conditional above to fail). Because Option 1
  // has the diversity requirement, it's implicitly a minimum number of SCTs
  // (specifically, 2), but that's not explicitly specified in the policy.

  // Option 2:
  // There is at least one embedded SCT from a log qualified at the time of
  //   check ...
  if (!has_valid_embedded_sct) {
    // Under Option 2, there weren't enough SCTs, and potentially under
    // Option 1, there weren't diverse enough SCTs. Try to signal the error
    // that is most easily fixed.
    return has_valid_nonembedded_sct
               ? CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS
               : CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  }

  // ... AND there is at least one embedded SCT from a Google Log once or
  //   currently qualified;
  // AND there is at least one embedded SCT from a non-Google Log once or
  //   currently qualified;
  // ...
  //
  // Note: This policy language is only enforced after the below issuance
  // date, as that's when the diversity policy first came into effect for
  // SCTs embedded in certificates.
  // The date when diverse SCTs requirement is effective from.
  // 2015-07-01 00:00:00 UTC.
  const base::Time kDiverseSCTRequirementStartDate =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1435708800);
  if (issuance_date >= kDiverseSCTRequirementStartDate &&
      !(has_embedded_google_sct && has_embedded_nongoogle_sct)) {
    // Note: This also covers the case for non-embedded SCTs, as it's only
    // possible to reach here if both sets are not diverse enough.
    return CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  }

  size_t lifetime_in_months = 0;
  bool has_partial_month = false;
  RoundedDownMonthDifference(cert.valid_start(), cert.valid_expiry(),
                             &lifetime_in_months, &has_partial_month);

  // ... AND the certificate embeds SCTs from AT LEAST the number of logs
  //   once or currently qualified shown in Table 1 of the CT Policy.
  size_t num_required_embedded_scts = 5;
  if (lifetime_in_months > 39 ||
      (lifetime_in_months == 39 && has_partial_month)) {
    num_required_embedded_scts = 5;
  } else if (lifetime_in_months > 27 ||
             (lifetime_in_months == 27 && has_partial_month)) {
    num_required_embedded_scts = 4;
  } else if (lifetime_in_months >= 15) {
    num_required_embedded_scts = 3;
  } else {
    num_required_embedded_scts = 2;
  }

  // Sort the embedded log IDs and remove duplicates, so that only a single
  // SCT from each log is accepted. This is to handle the case where a given
  // log returns different SCTs for the same precertificate (which is
  // permitted, but advised against).
  std::sort(embedded_log_ids.begin(), embedded_log_ids.end());
  auto sorted_end =
      std::unique(embedded_log_ids.begin(), embedded_log_ids.end());
  size_t num_embedded_scts =
      std::distance(embedded_log_ids.begin(), sorted_end);

  if (num_embedded_scts >= num_required_embedded_scts)
    return CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  // Under Option 2, there weren't enough SCTs, and potentially under Option
  // 1, there weren't diverse enough SCTs. Try to signal the error that is
  // most easily fixed.
  return has_valid_nonembedded_sct
             ? CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS
             : CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
}

}  // namespace certificate_transparency
