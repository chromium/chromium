// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_dogfood_feedback.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"

namespace omnibox_dogfood_feedback {

namespace {

// Dogfood only, so not translated.
constexpr char kDescriptionTemplate[] =
    "go/omnibox-dogfood\n\n"
    "DESCRIPTION of issue:\n\n"
    "REPRO STEPS:\n\n"
    "EXPECTED RESULTS:\n\n"
    "ACTUAL RESULTS (with screenshots or captures where available):\n\n";

// Identifier of the "Omnibox WebUI Googler" gFeedback bucket.
constexpr char kCategoryTag[] = "omnibox_webui";

// Appends the relevant entries of `overrides`, a list returned by
// base::FeatureList::GetFeatureOverrides(), to `out`.
void AppendRelevantOverrides(std::string_view state,
                             const std::string& overrides,
                             std::string& out) {
  const base::FeatureList* feature_list = base::FeatureList::GetInstance();
  for (std::string_view entry : base::SplitStringPiece(
           overrides, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // Entries look like "[*]FeatureName[<TrialName]". A "*" marks a feature
    // that keeps its default state but is associated with a trial, such as a
    // client in a Finch study's Default group.
    std::string_view name = entry.substr(0, entry.find('<'));
    std::string_view entry_state = state;
    if (name.starts_with('*')) {
      name.remove_prefix(1);
      entry_state = "default";
    }
    if (!internal::IsRelevantFeature(name)) {
      continue;
    }
    base::StrAppend(&out, {name, ": ", entry_state});
    if (base::FieldTrial* trial =
            feature_list->GetAssociatedFieldTrialByFeatureName(name)) {
      const std::string& group = trial->GetGroupNameWithoutActivation();
      base::StrAppend(&out, {" (", trial->trial_name(), "/", group, ")"});
      // Reads the params without activating the trial.
      base::FieldTrialParams params;
      if (base::FieldTrialParamAssociator::GetInstance()
              ->GetFieldTrialParamsWithoutFallback(trial->trial_name(), group,
                                                   &params)) {
        for (const auto& [key, value] : params) {
          base::StrAppend(&out, {" ", key, "=", value});
        }
      }
    }
    out += "\n";
  }
}

}  // namespace

void ShowFeedbackForm(BrowserWindowInterface* browser) {
  chrome::ShowFeedbackPage(browser, feedback::kFeedbackSourceOmnibox,
                           kDescriptionTemplate,
                           /*description_placeholder_text=*/"", kCategoryTag,
                           internal::GetDiagnostics());
}

namespace internal {

bool IsRelevantFeature(std::string_view name) {
  const std::string lower_name = base::ToLowerASCII(name);
  return lower_name.find("omnibox") != std::string::npos ||
         lower_name.find("webui") != std::string::npos;
}

std::string GetDiagnostics() {
  std::string enable_overrides;
  std::string disable_overrides;
  base::FeatureList::GetInstance()->GetFeatureOverrides(&enable_overrides,
                                                        &disable_overrides);
  std::string out = "Omnibox dogfood feedback diagnostics:\n";
  AppendRelevantOverrides("enabled", enable_overrides, out);
  AppendRelevantOverrides("disabled", disable_overrides, out);
  return out;
}

}  // namespace internal
}  // namespace omnibox_dogfood_feedback
