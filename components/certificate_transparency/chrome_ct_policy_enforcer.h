// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_CT_POLICY_ENFORCER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_CT_POLICY_ENFORCER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/cert/ct_policy_enforcer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  // to disqualification date.  |operated_by_google_logs| is a list of log IDs
  // operated by Google.  (Log IDs are the SHA-256 hash of the log's DER-encoded
  // SubjectPublicKeyInfo.)  |log_list_date| is the time at which the other two
  // arguments were generated.  Both lists of logs must be sorted by log ID.
  ChromeCTPolicyEnforcer(
      base::Time log_list_date,
      std::vector<std::pair<std::string, base::Time>> disqualified_logs,
      std::vector<std::string> operated_by_google_logs,
      std::map<std::string, OperatorHistoryEntry> log_operator_history);

  ~ChromeCTPolicyEnforcer() override;

  net::ct::CTPolicyCompliance CheckCompliance(
      net::X509Certificate* cert,
      const net::ct::SCTList& verified_scts,
      const net::NetLogWithSource& net_log) override;

  // Updates the list of logs used for compliance checks. |disqualified_logs| is
  // a map of log ID to disqualification date.  |operated_by_google_logs| is a
  // list of log IDs operated by Google
  void UpdateCTLogList(
      base::Time update_time,
      std::vector<std::pair<std::string, base::Time>> disqualified_logs,
      std::vector<std::string> operated_by_google_logs,
      std::map<std::string, OperatorHistoryEntry> log_operator_history);

  void SetClockForTesting(const base::Clock* clock) { clock_ = clock; }

  // TODO(https://crbug.com/999240): These are exposed to allow end-to-end
  // testing by higher layers (i.e. that the ChromeCTPolicyEnforcer is
  // correctly constructed). When either this issue or https://crbug.com/848277
  // are fixed, the configuration can be tested independently, and these can
  // be removed.
  const std::vector<std::string>& operated_by_google_logs_for_testing() {
    return operated_by_google_logs_;
  }
  const std::vector<std::pair<std::string, base::Time>>&
  disqualified_logs_for_testing() {
    return disqualified_logs_;
  }

  const std::map<std::string, OperatorHistoryEntry>&
  operator_history_for_testing() {
    return log_operator_history_;
  }

  void SetCTLogListAlwaysTimelyForTesting(bool always_timely) {
    ct_log_list_always_timely_for_testing_ = always_timely;
  }

  void SetOperatorHistoryForTesting(
      std::map<std::string, OperatorHistoryEntry> log_operator_history) {
    log_operator_history_ = std::move(log_operator_history);
  }

  void SetValidGoogleLogForTesting(const std::string& google_log) {
    valid_google_log_for_testing_ = google_log;
  }

  void SetDisqualifiedLogForTesting(
      const std::pair<std::string, base::Time>& disqualified_log) {
    disqualified_log_for_testing_ = disqualified_log;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeCTPolicyEnforcerTestBothPolicies,
                           IsLogDisqualifiedTimestamp);
  FRIEND_TEST_ALL_PREFIXES(ChromeCTPolicyEnforcerTestBothPolicies,
                           IsLogDisqualifiedReturnsFalseOnUnknownLog);
  // Returns true if the log identified by |log_id| (the SHA-256 hash of the
  // log's DER-encoded SPKI) has been disqualified, and sets
  // |*disqualification_date| to the date of disqualification. Any SCTs that
  // are embedded in certificates issued after |*disqualification_date| should
  // not be trusted, nor contribute to any uniqueness or freshness
  bool IsLogDisqualified(base::StringPiece log_id,
                         base::Time* disqualification_date) const;

  // Returns true if the log identified by |log_id| (the SHA-256 hash of the
  // log's DER-encoded SPKI) is operated by Google.
  bool IsLogOperatedByGoogle(base::StringPiece log_id) const;

  // Returns true if the supplied log data are fresh enough.
  bool IsLogDataTimely() const;

  net::ct::CTPolicyCompliance CheckCTPolicyCompliance(
      const net::X509Certificate& cert,
      const net::ct::SCTList& verified_scts) const;

  std::string GetOperatorForLog(std::string log_id, base::Time timestamp) const;

  // Map of SHA-256(SPKI) to log disqualification date.
  std::vector<std::pair<std::string, base::Time>> disqualified_logs_;

  // List of SHA-256(SPKI) for logs operated by Google.
  std::vector<std::string> operated_by_google_logs_;

  std::map<std::string, OperatorHistoryEntry> log_operator_history_;

  raw_ptr<const base::Clock> clock_;

  // The time at which |disqualified_logs_| and |operated_by_google_logs_| were
  // generated.
  base::Time log_list_date_;

  // If set, the CT log list will be considered timely regardless of its last
  // update time.
  bool ct_log_list_always_timely_for_testing_ = false;

  // If set, this log ID will be considered a valid, Google operated log.
  // Calling UpdateCTLogList clears this value if set.
  absl::optional<std::string> valid_google_log_for_testing_;

  // If set, this log ID will be considered a disqualified log, effective at the
  // specified time.
  // Calling UpdateCTLogList clears this value if set.
  absl::optional<std::pair<std::string, base::Time>>
      disqualified_log_for_testing_;
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_CT_POLICY_ENFORCER_H_
