// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_POLICY_FIND_AND_FILL_WITH_GEMINI_SETTINGS_POLICY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_POLICY_FIND_AND_FILL_WITH_GEMINI_SETTINGS_POLICY_HANDLER_H_

#include <memory>

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class GenAiDefaultSettingsPolicyHandler;
class PolicyErrorMap;
class PolicyMap;
class SimplePolicyHandler;

// Policy handler that determines the value of for the
// `FindAndFillWithGeminiSettings` pref. It applies the following rules:
// - If `GeminiSettings` is set to disabled, then
//   `kFindAndFillWithGeminiSettings` is disabled, too. If the
//   `FindAndFillWithGeminiSettings` is set and not disabled, an error is
//   emitted.
// - Otherwise, return the value of `FindAndFillWithGeminiSettings` or, if
//   unset, `GenAiDefaultSettings`.
class FindAndFillWithGeminiSettingsPolicyHandler
    : public IntRangePolicyHandler {
 public:
  explicit FindAndFillWithGeminiSettingsPolicyHandler(
      std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
          gen_ai_default_settings_policy_handler);
  FindAndFillWithGeminiSettingsPolicyHandler(
      const FindAndFillWithGeminiSettingsPolicyHandler&) = delete;
  FindAndFillWithGeminiSettingsPolicyHandler& operator=(
      const FindAndFillWithGeminiSettingsPolicyHandler&) = delete;
  ~FindAndFillWithGeminiSettingsPolicyHandler() override;

  // IntRangePolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
      gen_ai_default_settings_policy_handler_;
  const std::unique_ptr<SimplePolicyHandler> gemini_settings_policy_handler_;
};

}  // namespace policy

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_POLICY_FIND_AND_FILL_WITH_GEMINI_SETTINGS_POLICY_HANDLER_H_
