// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/buildflag.h"

namespace {
// Allow runtime override of the forced embedded page host.
std::string& GetForcedEmbeddedPageHostOverrideString() {
  static base::NoDestructor<std::string> override_string;
  return *override_string;
}
}  // namespace

namespace contextual_tasks {

// Enables the contextual tasks side panel while browsing.
BASE_FEATURE(kContextualTasks, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the pin button in the toolbar for contextual tasks.
BASE_FEATURE(kEnableContextualTasksPinButtonInToolbar,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kContextualTasksSendFullVersionListEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksSendContextualInputUploadType,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksUrlRedirectToAimUrl,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksUseStratusDarkModeColors,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, animates the caret.
BASE_FEATURE(kContextualTasksAnimatedCaret, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables energy effect in Nextbox. This works as a killswitch for the feature.
BASE_FEATURE(kEnergyEffectInNextbox, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksEnableFileHint, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksComposeboxJumpFix,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of a rounded clip-path for the composebox.
BASE_FEATURE(kContextualTasksRoundedClipPath, base::FEATURE_ENABLED_BY_DEFAULT);

// On android the menu still needs to be shown in all cases. Enable the feature
// everywhere else.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kContextualTasksHideMenuOnAiPage,
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kContextualTasksHideMenuOnAiPage,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kContextualTasksHideCloseButtonInVerticalTabs,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksVideoCitations, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksPdfCitations, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables lazy fetching of cluster info for multimodal queries.
BASE_FEATURE(kContextualTasksLazyFetchClusterInfo,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables custom UI for NLM.
BASE_FEATURE(kContextualTasksCustomNlmUi, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the back button expands the side panel.
BASE_FEATURE(kContextualTasksBackButtonExpandsSidePanel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of APC comparison for webpages in the recontextualization
// flow.
BASE_FEATURE(kContextualTasksWebpageApcComparison,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool GetIsContextualTasksPdfCitationsEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksPdfCitations);
}

bool GetIsContextualTasksLazyFetchClusterInfoEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksLazyFetchClusterInfo);
}

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

const base::FeatureParam<bool> kContextualTasksEnableCookiePrefetch(
    &kContextualTasks,
    "ContextualTasksEnableCookiePrefetch",
    false);

const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity(
    &kContextualTasksContext,
    "ContextualTasksContextOnlyUseTitles",
    false);

const base::FeatureParam<double> kTabSelectionScoreThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextTabSelectionScoreThreshold", 0.4};

const base::FeatureParam<double> kContentVisibilityThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextContentVisibilityThreshold", 0.7};

const base::FeatureParam<std::string> kQueryEmbeddingTask{
    &kContextualTasksContext, "ContextualTasksContextQueryEmbeddingTask", ""};

const base::FeatureParam<bool> kContextualTasksContextSmartTabSharing(
    &kContextualTasksContext,
    "ContextualTasksContextSmartTabSharing",
    false);

const base::FeatureParam<base::TimeDelta> kSmartTabSharingTabSelectionTimeout(
    &kContextualTasksContext,
    "ContextualTasksContextSmartTabSharingTabSelectionTimeout",
    base::Milliseconds(300));

const base::FeatureParam<double> kSmartTabSharingPromoScoreThreshold(
    &kContextualTasksContext,
    "ContextualTasksContextSmartTabSharingPromoScoreThreshold",
    0.6);

const base::FeatureParam<double> kContextualTasksContextLoggingSampleRate{
    &kContextualTasksContextLogging, "ContextualTasksContextLoggingSampleRate",
    1.0};

const base::FeatureParam<bool> kSendContextualInputUploadTypeInSearchUrl{
    &kContextualTasksSendContextualInputUploadType, "send_in_search_url", true};

const base::FeatureParam<bool> kSendContextualInputUploadTypeInAimRequest{
    &kContextualTasksSendContextualInputUploadType, "send_in_aim_request",
    true};

// Enables tab auto-chip for contextual tasks.
const base::FeatureParam<bool> kContextualTasksTabAutoSuggestionChipEnabled(
    &kContextualTasks,
    "ContextualTasksTabAutoSuggestionChipEnabled",
    true);

// The base URL for the AI page.
const base::FeatureParam<std::string> kContextualTasksAiPageUrl{
    &kContextualTasks, "contextual-tasks-ai-page-url",
    "https://www.google.com/search?udm=50&sourceid=chrome&ccb=1"};

const base::FeatureParam<std::string> kContextualTasksGeminiBaseUrl{
    &kContextualTasks, "contextual-tasks-gemini-base-url",
    "https://gemini.google.com/app/"};

// The host that any URL loaded in the embedded WebUi page will be routed to.
const base::FeatureParam<std::string> kContextualTasksForcedEmbeddedPageHost{
    &kContextualTasks, "contextual-tasks-forced-embedded-page-host", ""};

// The base domains for the sign in page.
const base::FeatureParam<std::string> kContextualTasksSignInDomains{
    &kContextualTasks, "contextual-tasks-sign-in-domains",
    "login.corp.google.com"};

constexpr base::FeatureParam<EntryPointOption>::Option kEntryPointOptions[] = {
    {EntryPointOption::kNoEntryPoint, "no-entry-point"},
    {EntryPointOption::kPageActionRevisit, "page-action-revisit"},
    {EntryPointOption::kToolbarRevisit, "toolbar-revisit"},
    {EntryPointOption::kToolbarPermanent, "toolbar-permanent"},
    {EntryPointOption::kToolbarEphemeralBranded, "toolbar-ephemeral-branded"}};

const base::FeatureParam<EntryPointOption> kShowEntryPoint(
    &kContextualTasks,
    "ContextualTasksEntryPoint",
    EntryPointOption::kNoEntryPoint,
    &kEntryPointOptions);

constexpr base::FeatureParam<ExpandButtonOption>::Option kExpandButtonOption[] =
    {{ExpandButtonOption::kSidePanelExpandButton, "side-panel-expand-button"},
     {ExpandButtonOption::kToolbarCloseButton, "toolbar-close-button"}};

const base::FeatureParam<ExpandButtonOption> kExpandButtonOptions(
    &kContextualTasks,
    "ContextualTasksExpandButtonOptions",
    ExpandButtonOption::kToolbarCloseButton,
    &kExpandButtonOption);

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
// Version 2.0: M146 respin launch candidate.
// Version 2.1: Enables stratus dark mode colors.
// Version 2.2: Added UI fixes for NLM.
// Version 2.3: UI fixes for transitions from search results.
// Version 2.4: Adds ability to hideInput/restoreInput
const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix{
    &kContextualTasks, "contextual-tasks-user-agent-suffix", "Cobrowsing/2.4"};

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

// The URL parameter name to check for NLM mode.
const base::FeatureParam<std::string> kContextualTasksNlmUrlParam{
    &kContextualTasksCustomNlmUi, "ContextualTasksNlmUrlParam", "ajid"};

const base::FeatureParam<std::string> kContextualTasksDisplayUrlScheme(
    &kContextualTasks,
    "ContextualTasksDisplayUrlScheme",
    "chrome");

const base::FeatureParam<std::string> kContextualTasksDisplayUrlHost(
    &kContextualTasks,
    "ContextualTasksDisplayUrlHost",
    "google.com");

const base::FeatureParam<std::string> kContextualTasksDisplayUrlPath(
    &kContextualTasks,
    "ContextualTasksDisplayUrlPath",
    "/search");

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

bool IsContextualTasksPinButtonInToolbarEnabled() {
  return base::FeatureList::IsEnabled(kEnableContextualTasksPinButtonInToolbar);
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

std::string GetContextualTasksGeminiBaseUrl() {
  return kContextualTasksGeminiBaseUrl.Get();
}

std::string GetContextualTasksDisplayUrlScheme() {
  return kContextualTasksDisplayUrlScheme.Get();
}

std::string GetContextualTasksDisplayUrlHost() {
  return kContextualTasksDisplayUrlHost.Get();
}

std::string GetContextualTasksDisplayUrlPath() {
  return kContextualTasksDisplayUrlPath.Get();
}

bool ShouldShowExpandedSecurityChip() {
  return kContextualTasksShowExpandedSecurityChip.Get();
}

std::string GetForcedEmbeddedPageHost() {
  std::string host = !GetForcedEmbeddedPageHostOverrideString().empty()
                         ? GetForcedEmbeddedPageHostOverrideString()
                         : kContextualTasksForcedEmbeddedPageHost.Get();

  // If there's a non-empty host, ensure that it is only ever going to a
  // google.com domain. If not, return the default empty string.
  // LINT.IfChange(AllowedHosts)
  if (!host.empty() && !(base::EndsWith(host, ".google.com") ||
                         base::EndsWith(host, ".googlers.com"))) {
    return kContextualTasksForcedEmbeddedPageHost.default_value;
  }
  // LINT.ThenChange(//depot/chromium/chrome/browser/resources/contextual_tasks/app.ts:AllowedHosts)

  return host;
}

void SetForcedEmbeddedPageHostOverride(const std::string& host) {
  GetForcedEmbeddedPageHostOverrideString() = host;
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

bool GetIsContextualTasksSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSuggestionsEnabled);
}

bool GetIsSmartTabSharingEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksContext) &&
         kContextualTasksContextSmartTabSharing.Get();
}

base::TimeDelta GetSmartTabSharingTabSelectionTimeout() {
  if (kSmartTabSharingTabSelectionTimeout.Get().is_positive()) {
    return kSmartTabSharingTabSelectionTimeout.Get();
  }
  return base::Milliseconds(300);
}

double GetSmartTabSharingPromoScoreThreshold() {
  if (kSmartTabSharingPromoScoreThreshold.Get() > 0.0 &&
      kSmartTabSharingPromoScoreThreshold.Get() <= 1.0) {
    return kSmartTabSharingPromoScoreThreshold.Get();
  }
  return 0.9;
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

std::string GetContextualTasksNlmUrlParam() {
  return kContextualTasksNlmUrlParam.Get();
}

bool IsCustomNlmUiEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksCustomNlmUi);
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

bool ShouldEnableCookiePrefetch() {
  return kContextualTasksEnableCookiePrefetch.Get();
}

bool ShouldEnableLockAndUnlockInputCapability() {
  return base::FeatureList::IsEnabled(kContextualTasks) &&
         kContextualTasksLockAndUnlockInputCapability.Get();
}

bool ShouldUseStratusDarkModeColors() {
  return base::FeatureList::IsEnabled(kContextualTasksUseStratusDarkModeColors);
}

bool GetEnableFileHint() {
  return base::FeatureList::IsEnabled(kContextualTasksEnableFileHint);
}

bool GetEnableComposeboxJumpFix() {
  return base::FeatureList::IsEnabled(kContextualTasksComposeboxJumpFix);
}

ExpandButtonOption GetExpandButtonOption() {
  return kExpandButtonOptions.Get();
}

bool IsRoundedClipPathEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksRoundedClipPath);
}

bool GetIsWebpageApcComparisonEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksWebpageApcComparison);
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

const char kContextualTasksSuggestionsEnabledName[] =
    "Contextual Tasks Suggestions Enabled";
const char kContextualTasksSuggestionsEnabledDescription[] =
    "Enables suggestions for contextual tasks.";

const char kContextualTasksBackButtonExpandsSidePanelName[] =
    "Contextual Tasks Back Button Expands Side Panel";
const char kContextualTasksBackButtonExpandsSidePanelDescription[] =
    "Enables expanding the side panel on back navigations.";

}  // namespace flag_descriptions

}  // namespace contextual_tasks
