// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_feedback.h"

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_dogfood_feedback.h"
#include "url/gurl.h"

namespace omnibox_everywhere {

namespace {

// Dogfood only, so not translated. The gFeedback rule prepends the dogfood
// bug title prefix automatically.
constexpr char kCategoryTag[] = "loomnibox_dogfood";
constexpr char kDescriptionTemplate[] =
    "go/new-loomni-bug\n\n"
    "DESCRIPTION of issue:\n\n"
    "REPRO STEPS:\n\n"
    "EXPECTED RESULTS:\n\n"
    "ACTUAL RESULTS (with screenshots or captures where available):\n\n";

}  // namespace

bool ShouldShowFeedback(const Profile* profile) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::FeatureList::IsEnabled(
             omnibox::kOmniboxEverywhereDogfoodFeedback) &&
         profile && chrome::CanShowFeedback(profile);
#else
  return false;
#endif
}

void ShowFeedbackForm(Profile* profile) {
  // Reuse the Omnibox WebUI dogfood diagnostics helper to attach overridden
  // omnibox/webui feature flags and Finch trial state without duplicating the
  // `FeatureList`/`FieldTrialParamAssociator` plumbing here.
  // TODO(crbug.com/579894893): Move `GetDiagnostics()` out of
  // `omnibox_dogfood_feedback::internal` into a shared omnibox helper.
  chrome::ShowFeedbackPage(
      GURL(), profile, feedback::kFeedbackSourceOmniboxEverywhere,
      kDescriptionTemplate,
      /*description_placeholder_text=*/"", kCategoryTag,
      omnibox_dogfood_feedback::internal::GetDiagnostics());
}

}  // namespace omnibox_everywhere
