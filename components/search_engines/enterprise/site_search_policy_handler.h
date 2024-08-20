// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SITE_SEARCH_POLICY_HANDLER_H_
#define COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SITE_SEARCH_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

#include <string>

#include "base/containers/flat_set.h"

namespace policy {

// ConfigurationPolicyHandler for the SiteSearchSettings policy.
class SiteSearchPolicyHandler : public SimpleSchemaValidatingPolicyHandler {
 public:
  // Fields in a site search engine entry.
  static const char kName[];
  static const char kShortcut[];
  static const char kUrl[];
  static const char kFeatured[];

  // The maximum number of site search providers to be defined via policy, to
  // avoid issues with very long lists.
  static const int kMaxSiteSearchProviders;

  // The maximum number of site search providers that can be marked as featured.
  static const int kMaxFeaturedProviders;

  explicit SiteSearchPolicyHandler(Schema schema);

  SiteSearchPolicyHandler(const SiteSearchPolicyHandler&) = delete;
  SiteSearchPolicyHandler& operator=(const SiteSearchPolicyHandler&) = delete;

  ~SiteSearchPolicyHandler() override;

  // SimpleSchemaValidatingPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The shortcuts corresponding to invalid entries that should not be written
  // to prefs. Used for caching validation results between |CheckPolicySettings|
  // and |ApplyPolicySettings|, so we don't need to replicate the validation
  // procedure in both methods.
  base::flat_set<std::string> ignored_shortcuts_;
};

}  // namespace policy

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SITE_SEARCH_POLICY_HANDLER_H_
