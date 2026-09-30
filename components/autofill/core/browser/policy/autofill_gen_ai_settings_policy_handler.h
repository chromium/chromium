// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_POLICY_AUTOFILL_GEN_AI_SETTINGS_POLICY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_POLICY_AUTOFILL_GEN_AI_SETTINGS_POLICY_HANDLER_H_

#include <memory>

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class GenAiDefaultSettingsPolicyHandler;
class PolicyMap;
class SimplePolicyHandler;

// Policy handler for `AutofillGenAiSettings`, the umbrella policy for the
// generative AI features in Autofill. It controls two prefs:
// - `kFindAndFillWithGeminiSettings`, which gates AtMemory and Ambient
//   Autofill, and
// - `kAutofillPredictionImprovementsEnterprisePolicyAllowed`, which gates
//   "Smarter form understanding".
//
// It applies the following rules:
// - If the Gemini app is disabled (either via `GeminiSettings` or via
//   `GenAiDefaultSettings` when `GeminiSettings` is unset), then
//   `kFindAndFillWithGeminiSettings` is forced to disabled. This dependency
//   exists because AtMemory and Ambient Autofill surface through the Gemini
//   app. "Smarter form understanding" does not, so
//   `kAutofillPredictionImprovementsEnterprisePolicyAllowed` is unaffected by
//   `GeminiSettings` (and no policy error is emitted on
//   `AutofillGenAiSettings`).
// - Otherwise, both prefs take the value of `AutofillGenAiSettings`. When
//   `AutofillGenAiSettings` is unset, the top-level
//   `GenAiDefaultSettingsPolicyHandler` (registered before this handler in
//   `configuration_policy_handler_list_factory`) applies the
//   `GenAiDefaultSettings` fallback to both prefs and lists
//   `AutofillGenAiSettings` in `GenAiDefaultSettings`'s `chrome://policy` info
//   message.
//
// `AutofillGenAiSettings` supersedes the legacy `AutofillPredictionSettings`
// policy. Because this handler runs after the simple policy handlers, the
// value written here wins when both policies are set.
class AutofillGenAiSettingsPolicyHandler : public IntRangePolicyHandler {
 public:
  explicit AutofillGenAiSettingsPolicyHandler(
      std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
          gen_ai_default_settings_policy_handler);
  AutofillGenAiSettingsPolicyHandler(
      const AutofillGenAiSettingsPolicyHandler&) = delete;
  AutofillGenAiSettingsPolicyHandler& operator=(
      const AutofillGenAiSettingsPolicyHandler&) = delete;
  ~AutofillGenAiSettingsPolicyHandler() override;

  // IntRangePolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Returns true if the Gemini app is disabled, by resolving `GeminiSettings`
  // and its `GenAiDefaultSettings` fallback into a scratch `PrefValueMap` via
  // the sub-handlers below. Using sub-handlers avoids duplicating
  // `GenAiDefaultSettings`'s value-mapping logic or relying on handler
  // execution order.
  bool IsGeminiDisabled(const PolicyMap& policies);

  // Sub-handlers used by `IsGeminiDisabled()` to resolve the effective value of
  // `kGeminiSettings` into a scratch `PrefValueMap`.
  const std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
      gen_ai_default_settings_policy_handler_;
  const std::unique_ptr<SimplePolicyHandler> gemini_settings_policy_handler_;
};

}  // namespace policy

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_POLICY_AUTOFILL_GEN_AI_SETTINGS_POLICY_HANDLER_H_
