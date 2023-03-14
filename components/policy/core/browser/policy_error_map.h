// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_POLICY_ERROR_MAP_H_
#define COMPONENTS_POLICY_CORE_BROWSER_POLICY_ERROR_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"

namespace policy {

// Collects error messages and their associated policies.
class POLICY_EXPORT PolicyErrorMap {
 public:
  struct POLICY_EXPORT Data {
    bool operator==(const Data& other) const;

    std::u16string message;
    PolicyMap::MessageType level;
  };
  typedef std::multimap<std::string, Data> PolicyMapType;
  typedef PolicyMapType::const_iterator const_iterator;

  class PendingError;

  PolicyErrorMap();
  PolicyErrorMap(const PolicyErrorMap&) = delete;
  PolicyErrorMap& operator=(const PolicyErrorMap&) = delete;
  virtual ~PolicyErrorMap();

  // Returns true when the errors logged are ready to be retrieved. It is always
  // safe to call AddError, but the other methods are only allowed once
  // IsReady is true. IsReady will be true once the UI message loop has started.
  bool IsReady() const;

  // Adds an entry with key |policy|, the error message corresponding to
  // |message_id| in grit/generated_resources.h and its error_path |error_path|
  // to the map.
  void AddError(
      const std::string& policy,
      int message_id,
      PolicyErrorPath error_path = {},
      PolicyMap::MessageType error_level = PolicyMap::MessageType::kError);

  // Adds an entry with key |policy|, the error message corresponding to
  // |message_id| in grit/generated_resources.h and its error_path |error_path|
  // to the map and replaces the placeholder within the error message with
  // |replacement_string|.
  void AddError(
      const std::string& policy,
      int message_id,
      const std::string& replacement_string,
      PolicyErrorPath error_path = {},
      PolicyMap::MessageType error_level = PolicyMap::MessageType::kError);

  // Same as AddError above but supports two replacement strings.
  void AddError(
      const std::string& policy,
      int message_id,
      const std::string& replacement_a,
      const std::string& replacement_b,
      PolicyErrorPath error_path = {},
      PolicyMap::MessageType error_level = PolicyMap::MessageType::kError);

  void AddError(
      const std::string& policy,
      int message_id,
      std::vector<std::string> replacements,
      PolicyErrorPath error_path = {},
      PolicyMap::MessageType error_level = PolicyMap::MessageType::kError);

  // Returns true if there is any error for |policy|.
  bool HasError(const std::string& policy);

  // Returns true if there is any fatal error (PolicyMap::MessageType::kError)
  // for |policy|. Returns false if |policy| only has non-fatal errors
  // (PolicyMap::MessageType::kInfo or PolicyMap::MessageType::kWarning) or no
  // errors at all.
  bool HasFatalError(const std::string& policy);

  // Returns all the error messages stored for |policy|, separated by a white
  // space. Returns an empty string if there are no errors for |policy|.
  std::u16string GetErrorMessages(const std::string& policy);

  // Returns all the error metadata stored for |policy| in a vector. Returns an
  // empty vector if there are no errors for |policy|.
  std::vector<Data> GetErrors(const std::string& policy);

  bool empty() const;
  size_t size();

  const_iterator begin();
  const_iterator end();

  void Clear();

 private:
  // Maps the error when ready, otherwise adds it to the pending errors list.
  void AddError(std::unique_ptr<PendingError> error);

  // Converts a PendingError into a |map_| entry.
  void Convert(PendingError* error);

  // Converts all pending errors to |map_| entries.
  void CheckReadyAndConvert();

  std::vector<std::unique_ptr<PendingError>> pending_;
  PolicyMapType map_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_ERROR_MAP_H_
