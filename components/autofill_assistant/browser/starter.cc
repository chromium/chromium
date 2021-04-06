// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include "base/feature_list.h"
#include "components/autofill_assistant/browser/features.h"

namespace autofill_assistant {

Starter::Starter(content::WebContents* web_contents,
                 StarterPlatformDelegate* platform_delegate)
    : content::WebContentsObserver(web_contents),
      platform_delegate_(platform_delegate) {
  OnSettingsChanged(
      platform_delegate_->GetProactiveHelpSettingEnabled(),
      platform_delegate_->GetMakeSearchesAndBrowsingBetterEnabled());
}

Starter::~Starter() = default;

void Starter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!fetch_trigger_scripts_on_navigation_) {
    return;
  }

  // TODO(arbesser): fetch trigger scripts when appropriate.
}

void Starter::OnSettingsChanged(bool proactive_help_setting_enabled,
                                bool msbb_setting_enabled) {
  fetch_trigger_scripts_on_navigation_ =
      base::FeatureList::IsEnabled(
          features::kAutofillAssistantInChromeTriggering) &&
      proactive_help_setting_enabled && msbb_setting_enabled;
}

}  // namespace autofill_assistant
