// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ENTERPRISE_NTP_SHORTCUTS_POLICY_HANDLER_H_
#define COMPONENTS_NTP_TILES_ENTERPRISE_NTP_SHORTCUTS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "url/gurl.h"

namespace policy {

// ConfigurationPolicyHandler for the NTPShortcuts policy.
class NTPShortcutsPolicyHandler : public SimpleSchemaValidatingPolicyHandler {
 public:
  // Fields in a NTPShortcut policy entry.
  static const char kName[];
  static const char kUrl[];
  static const char kAllowUserEdit[];
  static const char kAllowUserDelete[];

  // The maximum number of NTP shortcuts to be defined via policy.
  static const int kMaxNtpShortcuts;

  // The maximum length of a shortcut's name or url.
  static const int kMaxNtpShortcutTextLength;

  explicit NTPShortcutsPolicyHandler(Schema schema);

  NTPShortcutsPolicyHandler(const NTPShortcutsPolicyHandler&) = delete;
  NTPShortcutsPolicyHandler& operator=(const NTPShortcutsPolicyHandler&) =
      delete;

  ~NTPShortcutsPolicyHandler() override;

  // SimpleSchemaValidatingPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The urls corresponding to invalid entries that should not be written
  // to prefs. Used for caching validation results between |CheckPolicySettings|
  // and |ApplyPolicySettings|, so we don't need to replicate the validation
  // procedure in both methods.
  base::flat_set<GURL> ignored_urls_;
};

}  // namespace policy

#endif  // COMPONENTS_NTP_TILES_ENTERPRISE_NTP_SHORTCUTS_POLICY_HANDLER_H_
