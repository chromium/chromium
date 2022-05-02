// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_POLICY_ERROR_MAP_H_
#define COMPONENTS_POLICY_CORE_BROWSER_POLICY_ERROR_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/policy/policy_export.h"

namespace policy {

// Collects error messages and their associated policies.
class POLICY_EXPORT PolicyErrorMap {
 public:
  typedef std::multimap<std::string, std::u16string> PolicyMapType;
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

  // Adds an entry with key |policy| and the error message corresponding to
  // |message_id| in grit/generated_resources.h to the map.
  void AddError(const std::string& policy, int message_id);

  // Adds an entry with key |policy|, subkey |subkey|, and the error message
  // corresponding to |message_id| in grit/generated_resources.h to the map.
  void AddError(const std::string& policy,
                const std::string& subkey,
                int message_id);

  // Adds an entry with key |policy|, list index |index|, and the error message
  // corresponding to |message_id| in grit/generated_resources.h to the map.
  void AddError(const std::string& policy, int index, int message_id);

  // Adds an entry with key |policy| and the error message corresponding to
  // |message_id| in grit/generated_resources.h to the map and replaces the
  // placeholder within the error message with |replacement_string|.
  void AddError(const std::string& policy,
                int message_id,
                const std::string& replacement_string);

  // Same as AddError above but supports two replacement strings.
  void AddError(const std::string& policy,
                int message_id,
                const std::string& replacement_a,
                const std::string& replacement_b);

  // Adds an entry with key |policy|, subkey |subkey| and the error message
  // corresponding to |message_id| in grit/generated_resources.h to the map.
  // Replaces the placeholder in the error message with
  // |replacement_string|.
  void AddError(const std::string& policy,
                const std::string& subkey,
                int message_id,
                const std::string& replacement_string);

  // Adds an entry with key |policy|, list index |index| and the error message
  // corresponding to |message_id| in grit/generated_resources.h to the map.
  // Replaces the placeholder in the error message with |replacement_string|.
  void AddError(const std::string& policy,
                int index,
                int message_id,
                const std::string& replacement_string);

  // Adds an entry with key |policy|, the schema validation error location
  // |error_path|, and detailed error |message|.
  void AddError(const std::string& policy,
                const std::string& error_path,
                const std::string& message);

  // Returns true if there is any error for |policy|.
  bool HasError(const std::string& policy);

  // Returns all the error messages stored for |policy|, separated by a white
  // space. Returns an empty string if there are no errors for |policy|.
  std::u16string GetErrors(const std::string& policy);

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
