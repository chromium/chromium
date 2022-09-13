// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOAD_STATUS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOAD_STATUS_H_

#include <bitset>

#include "components/policy/policy_export.h"

namespace policy {

// UMA histogram enum for policy load status. Don't change existing constants,
// append additional constants to the end if needed.
enum PolicyLoadStatus {
  // Policy load attempt started. This gets logged for each policy load attempt
  // to get a baseline on the number of requests, and an arbitrary number of
  // the below status codes may get added in addition.
  POLICY_LOAD_STATUS_STARTED = 0,
  // System failed to determine whether there's policy.
  POLICY_LOAD_STATUS_QUERY_FAILED = 1,
  // No policy present.
  POLICY_LOAD_STATUS_NO_POLICY = 2,
  // Data inaccessible, such as non-local policy file.
  POLICY_LOAD_STATUS_INACCCESSIBLE = 3,
  // Data missing, such as policy file not present.
  POLICY_LOAD_STATUS_MISSING = 4,
  // Trying with Wow64 redirection disabled.
  POLICY_LOAD_STATUS_WOW64_REDIRECTION_DISABLED = 5,
  // Data read error, for example file reading errors.
  POLICY_LOAD_STATUS_READ_ERROR = 6,
  // Data too large to process.
  POLICY_LOAD_STATUS_TOO_BIG = 7,
  // Parse error.
  POLICY_LOAD_STATUS_PARSE_ERROR = 8,

  // This must stay last.
  POLICY_LOAD_STATUS_SIZE
};

// A helper for collecting statuses for a policy load operation.
class POLICY_EXPORT PolicyLoadStatusSampler {
 public:
  using StatusSet = std::bitset<POLICY_LOAD_STATUS_SIZE>;

  PolicyLoadStatusSampler();
  PolicyLoadStatusSampler(const PolicyLoadStatusSampler&) = delete;
  PolicyLoadStatusSampler& operator=(const PolicyLoadStatusSampler&) = delete;
  virtual ~PolicyLoadStatusSampler();

  // Adds a status code.
  void Add(PolicyLoadStatus status);

  // Returns a set with all statuses.
  const StatusSet& GetStatusSet() const { return status_bits_; }

 private:
  StatusSet status_bits_;
};

// A helper for generating policy load status UMA statistics. On destruction,
// records histogram samples for the collected status codes.
class POLICY_EXPORT PolicyLoadStatusUmaReporter
    : public PolicyLoadStatusSampler {
 public:
  PolicyLoadStatusUmaReporter();
  PolicyLoadStatusUmaReporter(const PolicyLoadStatusUmaReporter&) = delete;
  PolicyLoadStatusUmaReporter& operator=(const PolicyLoadStatusUmaReporter&) =
      delete;
  ~PolicyLoadStatusUmaReporter() override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOAD_STATUS_H_
