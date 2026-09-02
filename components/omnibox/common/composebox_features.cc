// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/composebox_features.h"

#include "build/build_config.h"

namespace omnibox {

namespace {
constexpr bool IS_IOS = !!BUILDFLAG(IS_IOS);
}  // namespace

BASE_FEATURE(kContextManagementInComposebox, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextMenuToolTips, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTabFaviconChipsToCoins, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kContextManagementInOmnibox, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextMenuAnimationLimiting, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kComposeboxSkillsContextualTasks,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kComposeboxSkillsNtp, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kComposeboxSkillsOmniboxEverywhere,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kComposeboxSkillsOmniboxPopup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kComposeboxRichImageSuggestions,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSuggestRequestSendsMultifileCgiParam,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<bool> kKeepMenuOpenOnTabSelectForRealbox(
    &kContextManagementInComposebox,
    "KeepMenuOpenOnTabSelectForRealboxComposebox",
    false);

const base::FeatureParam<bool>
    kContextManagementInComposeboxEnableTabDeselection(
        &kContextManagementInComposebox,
        "enable_tab_deselection",
        IS_IOS);

const base::FeatureParam<bool> kContextManagementInComposeboxTooltips(
    &kContextManagementInComposebox,
    "enable_context_menu_tooltips",
    false);

const base::FeatureParam<int> kContextMenuAnimationDailyLimit(
    &kContextMenuAnimationLimiting,
    "ContextMenuAnimationDailyLimit",
    5);

const base::FeatureParam<int> kContextMenuAnimationLifetimeLimit(
    &kContextMenuAnimationLimiting,
    "ContextMenuAnimationLifetimeLimit",
    20);

bool IsTabDeselectionInComposeboxEnabled() {
  return base::FeatureList::IsEnabled(kContextManagementInComposebox) &&
         kContextManagementInComposeboxEnableTabDeselection.Get();
}

}  // namespace omnibox
