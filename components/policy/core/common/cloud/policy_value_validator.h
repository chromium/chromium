// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_VALUE_VALIDATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_VALUE_VALIDATOR_H_

#include <string>
#include <vector>

namespace policy {

enum ValidationAction {
  kStore,
  kLoad,
};

struct ValueValidationIssue {
  enum Severity { kWarning, kError };

  std::string policy_name;
  Severity severity = kWarning;
  std::string message;

  bool operator==(ValueValidationIssue const& rhs) const {
    return policy_name == rhs.policy_name && severity == rhs.severity &&
           message == rhs.message;
  }
};

template <typename PayloadProto>
class PolicyValueValidator {
 public:
  PolicyValueValidator() = default;
  PolicyValueValidator(const PolicyValueValidator&) = delete;
  PolicyValueValidator& operator=(const PolicyValueValidator&) = delete;
  virtual ~PolicyValueValidator() = default;

  // Returns false if the value validation failed with errors.
  virtual bool ValidateValues(
      const PayloadProto& policy_payload,
      std::vector<ValueValidationIssue>* out_validation_issues) const = 0;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_VALUE_VALIDATOR_H_
