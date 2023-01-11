// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
  base::Value dict(base::Value::Type::DICTIONARY);
  // TODO(mattm): This double-wrapping of the certificate list is weird. Remove
  // this (probably requires updates to netlog-viewer).
  base::Value certificate_dict(base::Value::Type::DICTIONARY);
  certificate_dict.SetKey("certificates", net::NetLogX509CertificateList(cert));
  dict.SetKey("certificate", std::move(certificate_dict));
  dict.SetBoolKey("build_timely", build_timely);
  dict.SetStringKey("ct_compliance_status",
                    CTPolicyComplianceToString(compliance));
  return dict;
}

}  // namespace

OperatorHistoryEntry::OperatorHistoryEntry() = default;
OperatorHistoryEntry::~OperatorHistoryEntry() = default;
OperatorHistoryEntry::OperatorHistoryEntry(const OperatorHistoryEntry& other) =
    default;

ChromeCTPolicyEnforcer::ChromeCTPolicyEnforcer(
    base::Time log_list_date,
    std::vector<std::pair<std::string, base::Time>> disqualified_logs,
    std::vector<std::string> operated_by_google_logs,
    std::map<std::string, OperatorHistoryEntry> log_operator_history)
    : disqualified_logs_(std::move(disqualified_logs)),
      operated_by_google_logs_(std::move(operated_by_google_logs)),
      log_operator_history_(std::move(log_operator_history)),
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

void ChromeCTPolicyEnforcer::UpdateCTLogList(
    base::Time update_time,
    std::vector<std::pair<std::string, base::Time>> disqualified_logs,
    std::vector<std::string> operated_by_google_logs,
    std::map<std::string, OperatorHistoryEntry> log_operator_history) {
  log_list_date_ = update_time;
  disqualified_logs_ = std::move(disqualified_logs);
  operated_by_google_logs_ = std::move(operated_by_google_logs);
  log_operator_history_ = std::move(log_operator_history);

  if (valid_google_log_for_testing_.has_value()) {
    valid_google_log_for_testing_ = absl::nullopt;
  }
  if (disqualified_log_for_testing_.has_value()) {
    disqualified_log_for_testing_ = absl::nullopt;
  }
}

bool ChromeCTPolicyEnforcer::IsLogDisqualified(
    base::StringPiece log_id,
    base::Time* disqualification_date) const {
  CHECK_EQ(log_id.size(), crypto::kSHA256Length);

  if (disqualified_log_for_testing_.has_value() &&
      log_id == disqualified_log_for_testing_.value().first) {
    *disqualification_date = disqualified_log_for_testing_.value().second;
    return *disqualification_date < base::Time::Now();
  }

  auto p = std::lower_bound(
      std::begin(disqualified_logs_), std::end(disqualified_logs_), log_id,
      [](const auto& a, base::StringPiece b) { return a.first < b; });
  if (p == std::end(disqualified_logs_) || p->first != log_id) {
    return false;
  }
  *disqualification_date = p->second;
  if (base::Time::Now() < *disqualification_date) {
    return false;
  }
  return true;
}

bool ChromeCTPolicyEnforcer::IsLogOperatedByGoogle(
    base::StringPiece log_id) const {
  if (valid_google_log_for_testing_.has_value() &&
      log_id == valid_google_log_for_testing_.value()) {
    return true;
  }
  return std::binary_search(std::begin(operated_by_google_logs_),
                            std::end(operated_by_google_logs_), log_id);
}

bool ChromeCTPolicyEnforcer::IsLogDataTimely() const {
  if (ct_log_list_always_timely_for_testing_)
    return true;
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

  // Certificates issued after this date (April 15, 2022, OO:OO:OO GMT)
  // will be subject to the new CT policy, which:
  // -Removes the One Google log requirement.
  // -Introduces a log operator diversity (at least 2 SCTs that come from
  // different operators are required).
  // -Uses days for certificate lifetime calculations instead of rounding to
  // months.
  // Increases the SCT requirements for certificates with a lifetime between
  // 180 days and 15 months, from 2 to 3.
  // This conditional, and the pre-2022 policy logic can be removed after June
  // 1, 2023, since all publicly trusted certificates issued prior to the
  // policy change date will have expired by then.
  const base::Time kPolicyUpdateDate =
      base::Time::UnixEpoch() + base::Seconds(1649980800);
  bool use_2022_policy = issuance_date >= kPolicyUpdateDate;

  bool has_valid_google_sct = false;
  bool has_valid_nongoogle_sct = false;
  bool has_valid_embedded_sct = false;
  bool has_valid_nonembedded_sct = false;
  bool has_embedded_google_sct = false;
  bool has_embedded_nongoogle_sct = false;
  bool has_diverse_log_operators = false;
  std::vector<base::StringPiece> embedded_log_ids;
  std::string first_seen_operator;
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

    if (!use_2022_policy) {
      if (IsLogOperatedByGoogle(sct->log_id)) {
        has_valid_google_sct |= !is_disqualified;
        if (sct->origin == net::ct::SignedCertificateTimestamp::SCT_EMBEDDED)
          has_embedded_google_sct = true;
      } else {
        has_valid_nongoogle_sct |= !is_disqualified;
        if (sct->origin == net::ct::SignedCertificateTimestamp::SCT_EMBEDDED)
          has_embedded_nongoogle_sct = true;
      }
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

    if (use_2022_policy && !has_diverse_log_operators) {
      std::string sct_operator = GetOperatorForLog(sct->log_id, sct->timestamp);
      if (first_seen_operator.empty()) {
        first_seen_operator = sct_operator;
      } else {
        has_diverse_log_operators |= first_seen_operator != sct_operator;
      }
    }
  }

  // Option 1:
  // An SCT presented via the TLS extension OR embedded within a stapled OCSP
  //   response is from a log qualified at time of check;
  // With previous policy:
  //   AND there is at least one SCT from a Google Log that is qualified at
  //     time of check, presented via any method;
  //   AND there is at least one SCT from a non-Google Log that is qualified
  //     at the time of check, presented via any method.
  // With new policy:
  //   AND there are at least two SCTs from logs with different operators,
  //   presented by any method.
  //
  // Note: Because SCTs embedded via TLS or OCSP can be updated on the fly,
  // the issuance date is irrelevant, as any policy changes can be
  // accommodated.
  if (has_valid_nonembedded_sct) {
    if (use_2022_policy) {
      if (has_diverse_log_operators) {
        return CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
      }
    } else {
      if (has_valid_google_sct && has_valid_nongoogle_sct) {
        return CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
      }
    }
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

  size_t num_required_embedded_scts = 5;
  if (use_2022_policy) {
    // ... AND there are at least two SCTs from logs with different
    // operators ...
    if (!has_diverse_log_operators) {
      return CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
    }
    // ... AND the certificate embeds SCTs from AT LEAST the number of logs
    //   once or currently qualified shown in Table 1 of the CT Policy.
    base::TimeDelta lifetime = cert.valid_expiry() - cert.valid_start();
    if (lifetime > base::Days(180)) {
      num_required_embedded_scts = 3;
    } else {
      num_required_embedded_scts = 2;
    }
  } else {
    // ... AND there is at least one embedded SCT from a Google Log once or
    //   currently qualified;
    // AND there is at least one embedded SCT from a non-Google Log once or
    //   currently qualified;
    // ...
    if (!(has_embedded_google_sct && has_embedded_nongoogle_sct)) {
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

std::string ChromeCTPolicyEnforcer::GetOperatorForLog(
    std::string log_id,
    base::Time timestamp) const {
  if (valid_google_log_for_testing_.has_value() &&
      log_id == valid_google_log_for_testing_.value()) {
    return "Google";
  }
  DCHECK(log_operator_history_.find(log_id) != log_operator_history_.end());
  OperatorHistoryEntry log_history = log_operator_history_.at(log_id);
  for (auto operator_entry : log_history.previous_operators_) {
    if (timestamp < operator_entry.second)
      return operator_entry.first;
  }
  // Either the log has only ever had one operator, or the timestamp is after
  // the last operator change.
  return log_history.current_operator_;
}

}  // namespace certificate_transparency
