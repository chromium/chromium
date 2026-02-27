// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace contextual_tasks {

// Enables the contextual tasks side panel while browsing.
BASE_FEATURE(kContextualTasks, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of the kSearchResultsOAuth2Scope instead of the
// kChromeSyncOAuth2Scope.
BASE_FEATURE(kContextualTasksScopeChange, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables relevant context determination for contextual tasks.
BASE_FEATURE(kContextualTasksContext, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables integration with the server side context library.
BASE_FEATURE(kContextualTasksContextLibrary, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables quality logging for relevant context determination for contextual
// tasks.
BASE_FEATURE(kContextualTasksContextLogging, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables context menu settings for contextual tasks.
BASE_FEATURE(kContextualTasksContextMenu, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables suggestions for contextual tasks.
BASE_FEATURE(kContextualTasksSuggestionsEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksShowOnboardingTooltip,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Overrides the value of EntryPointEligibilitymanager::IsEligible to true.
BASE_FEATURE(kContextualTasksForceEntryPointEligibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Forces the country code to be US.
BASE_FEATURE(kContextualTasksForceCountryCodeUS,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksRemoveTasksWithoutThreadsOrTabAssociations,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNotifyZeroStateRenderedCapability,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksExpandButton, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksSendFullVersionListEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksUrlRedirectToAimUrl,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kContextualTasksLockAndUnlockInputCapability(
    &kContextualTasks,
    "ContextualTasksLockAndUnlockInputCapability",
    true);

const base::FeatureParam<bool> kContextualTasksEnableBasicMode(
    &kContextualTasks,
    "ContextualTasksEnableBasicMode",
    true);

const base::FeatureParam<bool> kContextualTasksBasicModeZOrder(
    &kContextualTasks,
    "ContextualTasksBasicModeZOrder",
    true);

const base::FeatureParam<bool> kContextualTasksEnableCookieSync(
    &kContextualTasks,
    "ContextualTasksEnableCookieSync",
    true);

const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity(
    &kContextualTasksContext,
    "ContextualTasksContextOnlyUseTitles",
    false);

const base::FeatureParam<double> kTabSelectionScoreThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextTabSelectionScoreThreshold", 0.8};

const base::FeatureParam<double> kContentVisibilityThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextContentVisibilityThreshold", 0.7};

const base::FeatureParam<double> kContextualTasksContextLoggingSampleRate{
    &kContextualTasksContextLogging, "ContextualTasksContextLoggingSampleRate",
    1.0};

// Enables tab auto-chip for contextual tasks.
const base::FeatureParam<bool> kContextualTasksTabAutoSuggestionChipEnabled(
    &kContextualTasks, "ContextualTasksTabAutoSuggestionChipEnabled", true);

// The base URL for the AI page.
const base::FeatureParam<std::string> kContextualTasksAiPageUrl{
    &kContextualTasks, "contextual-tasks-ai-page-url",
    "https://www.google.com/search?udm=50&sourceid=chrome"};

// The host that any URL loaded in the embedded WebUi page will be routed to.
const base::FeatureParam<std::string> kContextualTasksForcedEmbeddedPageHost{
    &kContextualTasks, "contextual-tasks-forced-embedded-page-host", ""};

// The base domains for the sign in page.
const base::FeatureParam<std::string> kContextualTasksSignInDomains{
    &kContextualTasks, "contextual-tasks-sign-in-domains",
    "accounts.google.com,login.corp.google.com"};

constexpr base::FeatureParam<EntryPointOption>::Option kEntryPointOptions[] = {
    {EntryPointOption::kNoEntryPoint, "no-entry-point"},
    {EntryPointOption::kPageActionRevisit, "page-action-revisit"},
    {EntryPointOption::kToolbarRevisit, "toolbar-revisit"},
    {EntryPointOption::kToolbarPermanent, "toolbar-permanent"}};

const base::FeatureParam<EntryPointOption> kShowEntryPoint(
    &kContextualTasks,
    "ContextualTasksEntryPoint",
    EntryPointOption::kToolbarRevisit,
    &kEntryPointOptions);

constexpr base::FeatureParam<ExpandButtonOption>::Option kExpandButtonOption[] =
    {{ExpandButtonOption::kSidePanelExpandButton, "side-panel-expand-button"},
     {ExpandButtonOption::kToolbarCloseButton, "toolbar-close-button"}};

const base::FeatureParam<ExpandButtonOption> kExpandButtonOptions(
    &kContextualTasks,
    "ContextualTasksExpandButtonOptions",
    ExpandButtonOption::kSidePanelExpandButton,
    &kExpandButtonOption);

const base::FeatureParam<bool> kTaskScopedSidePanel(
    &kContextualTasks,
    "ContextualTasksTaskScopedSidePanel",
    true);

const base::FeatureParam<bool> kOpenSidePanelOnLinkClicked(
    &kContextualTasks,
    "ContextualTasksOpenSidePanelOnLinkClicked",
    true);

const base::FeatureParam<bool> kEnableLensInContextualTasks(
    &kContextualTasks,
    "ContextualTasksEnableLensInContextualTasks",
    true);

const base::FeatureParam<bool> kForceGscInTabMode(
    &kContextualTasks,
    "ContextualTasksForceGscInTabMode",
    false);

// The user agent suffix to use for requests from the contextual tasks UI.
// Version 1.0: Initial version/implementation.
// Version 1.1: Client is capable of native suggestions.
// Version 1.2: Client is capable of composebox camouflage.
// Version 1.3: Bug fix for privacy notice on composebox camouflage.
const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix{
    &kContextualTasks, "contextual-tasks-user-agent-suffix",
    "Cobrowsing/1.3"};

const base::FeatureParam<bool> kEnableSteadyComposeboxVoiceSearch(
    &kContextualTasks,
    "ContextualTasksEnableSteadyComposeboxVoiceSearch",
    true);

const base::FeatureParam<bool> kEnableExpandedComposeboxVoiceSearch(
    &kContextualTasks,
    "ContextualTasksEnableExpandedComposeboxVoiceSearch",
    true);

// TODO(b/481079194): Remove `kAutoSubmitVoiceSearchQuery` and the code that
// respects its disabled state.
const base::FeatureParam<bool> kAutoSubmitVoiceSearchQuery(
    &kContextualTasks,
    "ContextualTasksAutoSubmitVoiceSearchQuery",
    true);

const base::FeatureParam<std::string> kContextualTasksHelpUrl(
    &kContextualTasks,
    "ContextualTasksHelpUrl",
    "https://support.google.com/websearch/");

const base::FeatureParam<bool> kEnableProtectedPageError(
    &kContextualTasks,
    "ContextualTasksEnableProtectedPageError",
    true);

const base::FeatureParam<bool> kEnableGhostLoader(&kContextualTasks,
                                                  "EnableGhostLoader",
                                                  true);

const base::FeatureParam<std::string> kContextualTasksOnboardingTooltipHelpUrl(
    &kContextualTasksShowOnboardingTooltip,
    "ContextualTasksOnboardingTooltipHelpUrl",
    "https://support.google.com/chrome?p=AI_tab_share");

const base::FeatureParam<int>
    kContextualTasksShowOnboardingTooltipSessionImpressionCap(
        &kContextualTasksShowOnboardingTooltip,
        "ContextualTasksShowOnboardingTooltipSessionImpressionCap",
        1);

const base::FeatureParam<int>
    kContextualTasksInactiveSidePanelKeepInCacheMinutes(
        &kContextualTasks,
        "ContextualTasksInactiveSidePanelKeepInCacheMinutes",
        1440);

const base::FeatureParam<int> kContextualTasksOnboardingTooltipDismissedCap(
    &kContextualTasksShowOnboardingTooltip,
    "ContextualTasksOnboardingTooltipDismissedCap",
    1);

const base::FeatureParam<int> kContextualTasksOnboardingTooltipImpressionDelay(
    &kContextualTasksShowOnboardingTooltip,
    "ContextualTasksOnboardingTooltipImpressionDelay",
    3000);

const base::FeatureParam<bool> kEnableContextualTasksSmartCompose(
    &kContextualTasks,
    "ContextualTasksEnableContextualTasksSmartCompose",
    true);

const base::FeatureParam<bool> kContextualTasksEnableNativeZeroStateSuggestions(
    &kContextualTasks,
    "ContextualTasksEnableNativeZeroStateSuggestions",
    true);

const base::FeatureParam<std::string> kContextualTasksDisplayUrlScheme(
    &kContextualTasks,
    "ContextualTasksDisplayUrlScheme",
    "chrome");

const base::FeatureParam<std::string> kContextualTasksDisplayUrlHost(
    &kContextualTasks,
    "ContextualTasksDisplayUrlHost",
    "googlesearch");

const base::FeatureParam<std::string> kContextualTasksDisplayUrlPath(
    &kContextualTasks,
    "ContextualTasksDisplayUrlPath",
    "/");

const base::FeatureParam<bool> kContextualTasksShowExpandedSecurityChip(
    &kContextualTasks,
    "ContextualTasksShowExpandedSecurityChip",
    false);

const base::FeatureParam<bool>
    kContextualTasksForceBasicModeIfOpeningThreadHistory(
        &kContextualTasks,
        "ContextualTasksForceBasicModeIfOpeningThreadHistory",
        true);

int GetContextualTasksShowOnboardingTooltipSessionImpressionCap() {
  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip)) {
    return 0;
  }
  return kContextualTasksShowOnboardingTooltipSessionImpressionCap.Get();
}

int GetContextualTasksOnboardingTooltipDismissedCap() {
  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip)) {
    return 0;
  }
  return kContextualTasksOnboardingTooltipDismissedCap.Get();
}

int GetContextualTasksOnboardingTooltipImpressionDelay() {
  return kContextualTasksOnboardingTooltipImpressionDelay.Get();
}

int ContextualTasksInactiveSidePanelKeepInCacheMinutes() {
  if (!base::FeatureList::IsEnabled(kContextualTasks)) {
    return 0;
  }
  return kContextualTasksInactiveSidePanelKeepInCacheMinutes.Get();
}

bool GetIsExpandedComposeboxVoiceSearchEnabled() {
  return kEnableExpandedComposeboxVoiceSearch.Get();
}

bool GetIsSteadyComposeboxVoiceSearchEnabled() {
  return kEnableSteadyComposeboxVoiceSearch.Get();
}

bool GetAutoSubmitVoiceSearchQuery() {
  return kAutoSubmitVoiceSearchQuery.Get();
}

bool GetIsProtectedPageErrorEnabled() {
  return kEnableProtectedPageError.Get();
}

bool GetIsGhostLoaderEnabled() {
  return kEnableGhostLoader.Get();
}

bool ShouldForceBasicModeIfOpeningThreadHistory() {
  return kContextualTasksForceBasicModeIfOpeningThreadHistory.Get();
}

bool ShouldForceGscInTabMode() {
  return kForceGscInTabMode.Get();
}

bool ShouldForceCountryCodeUS() {
  return base::FeatureList::IsEnabled(kContextualTasksForceCountryCodeUS);
}

std::string GetContextualTasksAiPageUrl() {
  return kContextualTasksAiPageUrl.Get();
}

std::string GetForcedEmbeddedPageHost() {
  std::string host = kContextualTasksForcedEmbeddedPageHost.Get();

  // If there's a non-empty host, ensure that it is only ever going to a
  // google.com domain. If not, return the default empty string.
  if (!host.empty() && !(base::EndsWith(host, ".google.com") ||
                         base::EndsWith(host, ".googlers.com"))) {
    return kContextualTasksForcedEmbeddedPageHost.default_value;
  }

  return host;
}

std::vector<std::string> GetContextualTasksSignInDomains() {
  return base::SplitString(kContextualTasksSignInDomains.Get(), ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

bool GetIsContextualTasksNextboxContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksContextMenu);
}

const base::FeatureParam<std::string> kContextualTasksNextboxImageFileTypes{
    &kContextualTasksContextMenu, "ContextualTasksNextboxImageFileTypes",
    "image/jpeg,image/png"};

const base::FeatureParam<std::string>
    kContextualTasksNextboxAttachmentFileTypes{
        &kContextualTasksContextMenu,
        "ContextualTasksNextboxAttachmentFileTypes",
        "text/plain,application/pdf"};

const base::FeatureParam<int> kContextualTasksNextboxMaxFileSize{
    &kContextualTasksContextMenu, "ContextualTasksNextboxMaxFileSize",
    20 * 1024 * 1024};

const base::FeatureParam<int> kContextualTasksNextboxMaxFileCount{
    &kContextualTasksContextMenu, "ContextualTasksNextboxMaxFileCount", 10};

bool GetIsContextualTasksSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSuggestionsEnabled);
}

bool GetIsTabAutoSuggestionChipEnabled() {
  return kContextualTasksTabAutoSuggestionChipEnabled.Get();
}

bool GetEnableLensInContextualTasks() {
  return base::FeatureList::IsEnabled(kContextualTasks) &&
         kEnableLensInContextualTasks.Get();
}

std::string GetContextualTasksUserAgentSuffix() {
  return kContextualTasksUserAgentSuffix.Get();
}

bool ShouldLogContextualTasksContextQuality() {
  if (!base::FeatureList::IsEnabled(kContextualTasksContextLogging)) {
    return false;
  }
  return base::RandDouble() <= kContextualTasksContextLoggingSampleRate.Get();
}

std::string GetContextualTasksOnboardingTooltipHelpUrl() {
  return kContextualTasksOnboardingTooltipHelpUrl.Get();
}

std::string GetContextualTasksHelpUrl() {
  return kContextualTasksHelpUrl.Get();
}

bool GetEnableContextualTasksSmartCompose() {
  return base::FeatureList::IsEnabled(kContextualTasks) &&
         kEnableContextualTasksSmartCompose.Get();
}

bool GetEnableNativeZeroStateSuggestions() {
  return kContextualTasksEnableNativeZeroStateSuggestions.Get();
}

bool ShouldUseSearchResultsScope() {
  return base::FeatureList::IsEnabled(kContextualTasksScopeChange);
}

bool GetIsBasicModeEnabled() {
  return kContextualTasksEnableBasicMode.Get();
}

bool ShouldEnableBasicModeZOrder() {
  return kContextualTasksBasicModeZOrder.Get();
}

bool ShouldEnableCookieSync() {
  return kContextualTasksEnableCookieSync.Get();
}

bool ShouldEnableLockAndUnlockInputCapability() {
  return base::FeatureList::IsEnabled(kContextualTasks) &&
         kContextualTasksLockAndUnlockInputCapability.Get();
}

ExpandButtonOption GetExpandButtonOption() {
  return kExpandButtonOptions.Get();
}

namespace flag_descriptions {

const char kContextualTasksName[] = "Contextual Tasks";
const char kContextualTasksDescription[] =
    "Enable the contextual tasks feature.";

const char kContextualTasksContextName[] = "Contextual Tasks Context";
const char kContextualTasksContextDescription[] =
    "Enables relevant context determination for contextual tasks.";

const char kContextualTasksContextLibraryName[] =
    "Contextual Tasks Context Library";
const char kContextualTasksContextLibraryDescription[] =
    "Enables integration with the server side context library.";

const char kContextualTasksExpandButtonName[] =
    "Contextual Tasks Expand Button";
const char kContextualTasksExpandButtonDescription[] =
    "Replace the overflow menu in the side panel with a button to move the "
    "thread to a new tab.";

const char kContextualTasksSuggestionsEnabledName[] =
    "Contextual Tasks Suggestions Enabled";
const char kContextualTasksSuggestionsEnabledDescription[] =
    "Enables suggestions for contextual tasks.";

}  // namespace flag_descriptions

}  // namespace contextual_tasks
