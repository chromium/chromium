// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SITE_SEARCH_POLICY_HANDLER_H_
#define COMPONENTS_SEARCH_ENGINES_SITE_SEARCH_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the SiteSearchSettings policy.
class SiteSearchPolicyHandler : public SimpleSchemaValidatingPolicyHandler {
 public:
  // Fields in a site search engine entry.
  static const char kName[];
  static const char kShortcut[];
  static const char kUrl[];

  explicit SiteSearchPolicyHandler(Schema schema);

  SiteSearchPolicyHandler(const SiteSearchPolicyHandler&) = delete;
  SiteSearchPolicyHandler& operator=(const SiteSearchPolicyHandler&) = delete;

  ~SiteSearchPolicyHandler() override;

  // SimpleSchemaValidatingPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // COMPONENTS_SEARCH_ENGINES_SITE_SEARCH_POLICY_HANDLER_H_
