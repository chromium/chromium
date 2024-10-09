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
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

using net::ct::CTPolicyCompliance;

namespace certificate_transparency {

namespace {

base::Value::Dict NetLogCertComplianceCheckResultParams(
    net::X509Certificate* cert,
    bool build_timely,
    CTPolicyCompliance compliance) {
  base::Value::Dict dict;
  dict.Set("build_timely", build_timely);
  dict.Set("ct_compliance_status", CTPolicyComplianceToString(compliance));
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
    std::map<std::string, LogInfo> log_info,
    bool enable_static_ct_api_enforcement)
    : disqualified_logs_(std::move(disqualified_logs)),
      log_info_(std::move(log_info)),
      log_list_date_(log_list_date),
      enable_static_ct_api_enforcement_(enable_static_ct_api_enforcement) {}

ChromeCTPolicyEnforcer::~ChromeCTPolicyEnforcer() = default;

CTPolicyCompliance ChromeCTPolicyEnforcer::CheckCompliance(
    net::X509Certificate* cert,
    const net::ct::SCTList& verified_scts,
    base::Time current_time,
    const net::NetLogWithSource& net_log) const {
  // If the build is not timely, no certificate is considered compliant
  // with CT policy. The reasoning is that, for example, a log might
  // have been pulled and is no longer considered valid; thus, a client
  // needs up-to-date information about logs to consider certificates to
  // be compliant with policy.
  bool build_timely = IsLogDataTimely(current_time);
  CTPolicyCompliance compliance;
  if (!build_timely) {
    compliance = CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY;
  } else {
    compliance = CheckCTPolicyCompliance(*cert, verified_scts, current_time);
  }

  net_log.AddEvent(net::NetLogEventType::CERT_CT_COMPLIANCE_CHECKED, [&] {
    return NetLogCertComplianceCheckResultParams(cert, build_timely,
                                                 compliance);
  });

  return compliance;
}

std::optional<base::Time> ChromeCTPolicyEnforcer::GetLogDisqualificationTime(
    std::string_view log_id) const {
  CHECK_EQ(log_id.size(), crypto::kSHA256Length);

  auto p = std::lower_bound(
      std::begin(disqualified_logs_), std::end(disqualified_logs_), log_id,
      [](const auto& a, std::string_view b) { return a.first < b; });
  if (p == std::end(disqualified_logs_) || p->first != log_id) {
    return std::nullopt;
  }
  return p->second;
}

bool ChromeCTPolicyEnforcer::IsCtEnabled() const {
  return true;
}

bool ChromeCTPolicyEnforcer::IsLogDisqualified(
    std::string_view log_id,
    base::Time current_time,
    base::Time* out_disqualification_date) const {
  std::optional<base::Time> disqualification_date =
      GetLogDisqualificationTime(log_id);
  if (!disqualification_date.has_value()) {
    return false;
  }
  *out_disqualification_date = disqualification_date.value();
  return current_time >= disqualification_date.value();
}

bool ChromeCTPolicyEnforcer::IsLogDataTimely(base::Time current_time) const {
  // We consider built-in information to be timely for 10 weeks.
  return (current_time - log_list_date_).InDays() < 70 /* 10 weeks */;
}

// Evaluates against the "on-or-after 15 April 2022" policy specified at
// https://googlechrome.github.io/CertificateTransparency/ct_policy.html
// (No certificate issued before that date could still be valid.)
CTPolicyCompliance ChromeCTPolicyEnforcer::CheckCTPolicyCompliance(
    const net::X509Certificate& cert,
    const net::ct::SCTList& verified_scts,
    base::Time current_time) const {
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
    if (IsLogDisqualified(sct->log_id, current_time, &unused)) {
      continue;
    }
    issuance_date = std::min(sct->timestamp, issuance_date);
  }

  bool has_valid_embedded_sct = false;
  bool has_valid_nonembedded_sct = false;
  bool has_diverse_log_operators = false;
  bool has_rfc6962_log = false;
  std::vector<std::string_view> embedded_log_ids;
  std::string first_seen_operator;
  for (const auto& sct : verified_scts) {
    base::Time disqualification_date;
    bool is_disqualified =
        IsLogDisqualified(sct->log_id, current_time, &disqualification_date);
    if (is_disqualified &&
        sct->origin != net::ct::SignedCertificateTimestamp::SCT_EMBEDDED) {
      // For OCSP and TLS delivered SCTs, only SCTs that are valid at the
      // time of check are accepted.
      continue;
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

    if (!has_diverse_log_operators) {
      std::string sct_operator = GetOperatorForLog(sct->log_id, sct->timestamp);
      if (first_seen_operator.empty()) {
        first_seen_operator = sct_operator;
      } else {
        has_diverse_log_operators |= first_seen_operator != sct_operator;
      }
    }

    if (enable_static_ct_api_enforcement_) {
      has_rfc6962_log |= (GetLogType(sct->log_id) ==
                          network::mojom::CTLogInfo::LogType::kRFC6962);
    }
  }

  // Option 1:
  // An SCT presented via the TLS extension OR embedded within a stapled OCSP
  //   response is from a log qualified at time of check;
  // AND there are at least two SCTs from logs with different operators,
  //   presented by any method.
  //
  // Note: Because SCTs embedded via TLS or OCSP can be updated on the fly,
  // the issuance date is irrelevant, as any policy changes can be
  // accommodated.
  if (has_valid_nonembedded_sct && has_diverse_log_operators &&
      (!enable_static_ct_api_enforcement_ || has_rfc6962_log)) {
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

  size_t num_required_embedded_scts = 5;
  // ... AND there are at least two SCTs from logs with different
  // operators ...
  if (!has_diverse_log_operators) {
    return CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  }

  // ... AND at least one of the SCTs must come from an RFC6962 log.
  if (enable_static_ct_api_enforcement_ && !has_rfc6962_log) {
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

  // Sort the embedded log IDs and remove duplicates, so that only a single
  // SCT from each log is accepted. This is to handle the case where a given
  // log returns different SCTs for the same precertificate (which is
  // permitted, but advised against).
  std::sort(embedded_log_ids.begin(), embedded_log_ids.end());
  auto sorted_end =
      std::unique(embedded_log_ids.begin(), embedded_log_ids.end());
  size_t num_embedded_scts =
      std::distance(embedded_log_ids.begin(), sorted_end);

  if (num_embedded_scts >= num_required_embedded_scts) {
    return CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  }

  // Under Option 2, there weren't enough SCTs, and potentially under Option
  // 1, there weren't diverse enough SCTs. Try to signal the error that is
  // most easily fixed.
  return has_valid_nonembedded_sct
             ? CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS
             : CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
}

std::string ChromeCTPolicyEnforcer::GetOperatorForLog(
    const std::string& log_id,
    base::Time timestamp) const {
  DCHECK(log_info_.find(log_id) != log_info_.end());
  const OperatorHistoryEntry& log_history =
      log_info_.at(log_id).operator_history;
  for (auto operator_entry : log_history.previous_operators_) {
    if (timestamp < operator_entry.second)
      return operator_entry.first;
  }
  // Either the log has only ever had one operator, or the timestamp is after
  // the last operator change.
  return log_history.current_operator_;
}

network::mojom::CTLogInfo::LogType ChromeCTPolicyEnforcer::GetLogType(
    const std::string& log_id) const {
  DCHECK(log_info_.find(log_id) != log_info_.end());
  return log_info_.at(log_id).log_type;
}

}  // namespace certificate_transparency
