// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ENTERPRISE_DEFAULT_SEARCH_POLICY_HANDLER_H_
#define COMPONENTS_SEARCH_ENGINES_ENTERPRISE_DEFAULT_SEARCH_POLICY_HANDLER_H_

#include <memory>

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the default search policies.
class DefaultSearchPolicyHandler : public ConfigurationPolicyHandler {
 public:
  DefaultSearchPolicyHandler();

  DefaultSearchPolicyHandler(const DefaultSearchPolicyHandler&) = delete;
  DefaultSearchPolicyHandler& operator=(const DefaultSearchPolicyHandler&) =
      delete;

  ~DefaultSearchPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Checks that value type is valid for each policy and returns whether all of
  // the policies are valid.
  bool CheckIndividualPolicies(const PolicyMap& policies,
                               PolicyErrorMap* errors);

  // Returns whether there is a value for |policy_name| in |policies|.
  bool HasDefaultSearchPolicy(const PolicyMap& policies,
                              const char* policy_name);

  // Returns whether any default search policies are specified in |policies|.
  bool AnyDefaultSearchPoliciesSpecified(const PolicyMap& policies);

  // Returns whether the default search provider policy has a value.
  bool DefaultSearchProviderPolicyIsSet(const PolicyMap& policies);

  // Returns whether the default search provider is disabled.
  bool DefaultSearchProviderIsDisabled(const PolicyMap& policies);

  // Returns whether the default search URL is set and valid.  On success, both
  // outparams (which must be non-NULL) are filled with the search URL.
  bool DefaultSearchURLIsValid(const PolicyMap& policies,
                               const base::Value** url_value,
                               std::string* url_string);

  // Make sure that the |path| is present in |prefs_|.  If not, set it to
  // a blank string.
  void EnsureStringPrefExists(PrefValueMap* prefs, const std::string& path);

  // Make sure that the |path| is present in |prefs_| and is a List.  If
  // not, set it to an empty list.
  void EnsureListPrefExists(PrefValueMap* prefs, const std::string& path);
};

}  // namespace policy

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_DEFAULT_SEARCH_POLICY_HANDLER_H_
