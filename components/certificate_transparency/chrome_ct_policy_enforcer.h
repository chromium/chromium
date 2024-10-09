// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_CT_POLICY_ENFORCER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_CT_POLICY_ENFORCER_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/cert/ct_policy_enforcer.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"

namespace certificate_transparency {

struct COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) OperatorHistoryEntry {
  // Name of the current operator for the log.
  std::string current_operator_;
  // Vector of previous operators (if any) for the log, represented as pairs of
  // operator name and time when they stopped operating the log.
  std::vector<std::pair<std::string, base::Time>> previous_operators_;

  OperatorHistoryEntry();
  ~OperatorHistoryEntry();
  OperatorHistoryEntry(const OperatorHistoryEntry& other);
};

struct COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) LogInfo {
  // Operator history for this log.
  OperatorHistoryEntry operator_history;
  // Type of the log.
  network::mojom::CTLogInfo::LogType log_type;
};

// A CTPolicyEnforcer that enforces the "Certificate Transparency in Chrome"
// policies detailed at
// https://github.com/chromium/ct-policy/blob/master/ct_policy.md
//
// This should only be used when there is a reliable, rapid update mechanism
// for the set of known, qualified logs - either through a reliable binary
// updating mechanism or through out-of-band delivery. See
// //net/docs/certificate-transparency.md for more details.
class COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) ChromeCTPolicyEnforcer
    : public net::CTPolicyEnforcer {
 public:
  // |logs| is a list of Certificate Transparency logs.  Data about each log is
  // needed to apply Chrome's policies. |disqualified_logs| is a map of log ID
  // to disqualification date.  (Log IDs are the SHA-256 hash of the log's
  // DER-encoded SubjectPublicKeyInfo.)  |log_list_date| is the time at which
  // the other two arguments were generated.  Both lists of logs must be sorted
  // by log ID. |log_info| contains operator history and log types of the logs.
  ChromeCTPolicyEnforcer(
      base::Time log_list_date,
      std::vector<std::pair<std::string, base::Time>> disqualified_logs,
      std::map<std::string, LogInfo> log_info,
      bool enable_static_ct_api_enforcement);

  net::ct::CTPolicyCompliance CheckCompliance(
      net::X509Certificate* cert,
      const net::ct::SCTList& verified_scts,
      base::Time current_time,
      const net::NetLogWithSource& net_log) const override;

  std::optional<base::Time> GetLogDisqualificationTime(
      std::string_view log_id) const override;

  bool IsCtEnabled() const override;

  // TODO(crbug.com/41479068): These are exposed to allow end-to-end
  // testing by higher layers (i.e. that the ChromeCTPolicyEnforcer is
  // correctly constructed). When either this issue or https://crbug.com/848277
  // are fixed, the configuration can be tested independently, and these can
  // be removed.
  const std::vector<std::pair<std::string, base::Time>>&
  disqualified_logs_for_testing() {
    return disqualified_logs_;
  }

  const std::map<std::string, LogInfo>& log_info_for_testing() const {
    return log_info_;
  }

 protected:
  ~ChromeCTPolicyEnforcer() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeCTPolicyEnforcerTest,
                           IsLogDisqualifiedTimestamp);
  FRIEND_TEST_ALL_PREFIXES(ChromeCTPolicyEnforcerTest,
                           IsLogDisqualifiedReturnsFalseOnUnknownLog);

  // Returns true if the log identified by |log_id| (the SHA-256 hash of the
  // log's DER-encoded SPKI) has been disqualified, and sets
  // |*disqualification_date| to the date of disqualification. Any SCTs that
  // are embedded in certificates issued after |*disqualification_date| should
  // not be trusted, nor contribute to any uniqueness or freshness
  bool IsLogDisqualified(std::string_view log_id,
                         base::Time current_time,
                         base::Time* disqualification_date) const;

  // Returns true if the supplied log data are fresh enough.
  bool IsLogDataTimely(base::Time current_time) const;

  net::ct::CTPolicyCompliance CheckCTPolicyCompliance(
      const net::X509Certificate& cert,
      const net::ct::SCTList& verified_scts,
      base::Time current_time) const;

  std::string GetOperatorForLog(const std::string& log_id,
                                base::Time timestamp) const;

  network::mojom::CTLogInfo::LogType GetLogType(
      const std::string& log_id) const;

  // Map of SHA-256(SPKI) to log disqualification date.
  const std::vector<std::pair<std::string, base::Time>> disqualified_logs_;

  const std::map<std::string, LogInfo> log_info_;

  // The time at which |disqualified_logs_| and |log_operator_history_| were
  // generated.
  const base::Time log_list_date_;

  const bool enable_static_ct_api_enforcement_;
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_CT_POLICY_ENFORCER_H_
