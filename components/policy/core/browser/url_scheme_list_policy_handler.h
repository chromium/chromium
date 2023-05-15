// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_SCHEME_LIST_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_SCHEME_LIST_POLICY_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace policy {

// Maps policy to pref like TypeCheckingPolicyHandler while ensuring that the
// value is a list of urls that follow the url format which documented at
// http://www.chromium.org/administrators/url-blocklist-filter-format
class POLICY_EXPORT URLSchemeListPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  URLSchemeListPolicyHandler(const char* policy_name, const char* pref_path);
  URLSchemeListPolicyHandler(const URLSchemeListPolicyHandler&) = delete;
  URLSchemeListPolicyHandler& operator=(const URLSchemeListPolicyHandler&) =
      delete;
  ~URLSchemeListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 protected:
  virtual size_t max_items();
  virtual bool ValidatePolicyEntry(const std::string* policy);

 private:
  const char* pref_path_;

  FRIEND_TEST_ALL_PREFIXES(URLSchemeListPolicyHandlerTest, ValidatePolicyEntry);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_SCHEME_LIST_POLICY_HANDLER_H_
