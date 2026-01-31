// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_SEARCH_CONTENT_SHARING_POLICY_HANDLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_SEARCH_CONTENT_SHARING_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace contextual_search {

// Handles the `SearchContentSharing` policy and sets the prefs for the three
// related Lens policies if the policy is set.
class SearchContentSharingPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  SearchContentSharingPolicyHandler(
      std::string pref_path_to_override,
      bool convert_policy_value_to_enabled_boolean);
  SearchContentSharingPolicyHandler& operator=(
      const SearchContentSharingPolicyHandler&) = delete;
  SearchContentSharingPolicyHandler(const SearchContentSharingPolicyHandler&) =
      delete;
  ~SearchContentSharingPolicyHandler() override;

  // policy::ConfigurationPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Additional pref path to set when the policy is set.
  std::string pref_path_to_override_;

  // If true, convert the policy value to a boolean, with true meaning enabled.
  bool convert_policy_value_to_enabled_boolean_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_SEARCH_CONTENT_SHARING_POLICY_HANDLER_H_
