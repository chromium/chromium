// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SEARCH_AGGREGATOR_POLICY_HANDLER_H_
#define COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SEARCH_AGGREGATOR_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the EnterpriseSearchAggregatorSettings policy.
class SearchAggregatorPolicyHandler
    : public SimpleSchemaValidatingPolicyHandler {
 public:
  // Fields in a search aggregator entry.
  static const char kIconUrl[];
  static const char kName[];
  static const char kSearchUrl[];
  static const char kShortcut[];
  static const char kSuggestUrl[];

  explicit SearchAggregatorPolicyHandler(Schema schema);

  SearchAggregatorPolicyHandler(const SearchAggregatorPolicyHandler&) = delete;
  SearchAggregatorPolicyHandler& operator=(
      const SearchAggregatorPolicyHandler&) = delete;

  ~SearchAggregatorPolicyHandler() override;

  // SimpleSchemaValidatingPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SEARCH_AGGREGATOR_POLICY_HANDLER_H_
