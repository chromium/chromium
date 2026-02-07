// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_prefs.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace default_browser {

void SetInfoBarShownRecently() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetTime(prefs::kPinInfoBarLastShown, base::Time::Now());
  local_state->SetInteger(
      prefs::kPinInfoBarTimesShown,
      local_state->GetInteger(prefs::kPinInfoBarTimesShown) + 1);
}

bool InfoBarShownRecentlyOrMaxTimes() {
  PrefService* local_state = g_browser_process->local_state();
  const auto last_shown = local_state->GetTime(prefs::kPinInfoBarLastShown);

  int reprompt_days = kPinInfoBarRepromptDays;
  int max_prompt_count = kPinInfoBarMaxPromptCount;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    reprompt_days = features::kSeparateDefaultAndPinPromptPinCooldownDays.Get();
    max_prompt_count = features::kSeparateDefaultAndPinPromptPinMaxCount.Get();
  }
#endif

  if (last_shown + base::Days(reprompt_days) > base::Time::Now()) {
    return true;
  }
  return local_state->GetInteger(prefs::kPinInfoBarTimesShown) >=
         max_prompt_count;
}

}  // namespace default_browser
