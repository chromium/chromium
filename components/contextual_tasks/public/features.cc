// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/buildflag.h"
#include "ui/base/device_form_factor.h"

namespace {
// Allow runtime override of the forced embedded page host.
std::optional<contextual_tasks::HostOverride>&
GetForcedEmbeddedPageHostOverride() {
  static base::NoDestructor<std::optional<contextual_tasks::HostOverride>>
      override_host;
  return *override_host;
}

// Allows tests to override the conditions for having sticky conversation.
std::optional<bool> g_sticky_conversation_enabled_override;

}  // namespace

namespace contextual_tasks {

// Enables the contextual tasks side panel while browsing.
BASE_FEATURE(kContextualTasks, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextualTasksPrivateApiNoAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the contextual tasks side panel infrastructure without full product
// capabilities.
BASE_FEATURE(kContextualTasksSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the branded entry point for contextual tasks.
BASE_FEATURE(kContextualTasksEphemeralBrandedEntryPoint,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables extra OAuth scopes for contextual tasks.
BASE_FEATURE(kContextualTasksExtraOauthScopes,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Google Drive OAuth scope for contextual tasks.
BASE_FEATURE(kContextualTasksDriveOAuthScope, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the pin button in the toolbar for contextual tasks.
BASE_FEATURE(kEnableContextualTasksPinButtonInToolbar,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Keeps the ephemeral contextual tasks button visible even when the permanent
// button is pinned in the toolbar.
BASE_FEATURE(kEphemeralPinningVisibleWhenPermanentlyPinned,
             base::FEATURE_DISABLED_BY_DEFAULT);


// Enables relevant context determination for contextual tasks.
BASE_FEATURE(kContextualTasksContext, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksSearchQuery, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables multi-turn tab relevance model for contextual tasks.
BASE_FEATURE(kContextualTasksContextMultiTurnTabRelevance,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables whether the option to enable smart tab sharing by default is enabled.
BASE_FEATURE(kContextualTasksContextSmartTabSharingDefaultOnAvailability,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables integration with the server side context library.
BASE_FEATURE(kContextualTasksContextLibrary, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the script tools execution pipeline, including tab ID injection and
// transient task overlays.
BASE_FEATURE(kContextualTasksScriptTools, base::FEATURE_DISABLED_BY_DEFAULT);

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

// Bypasses the dismissed cap for contextual tasks.
BASE_FEATURE(kContextualTasksBypassDismissedCap,
             base::FEATURE_DISABLED_BY_DEFAULT);

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


// If enabled, animates the caret.
BASE_FEATURE(kContextualTasksAnimatedCaret, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables energy effect in Nextbox. This works as a killswitch for the feature.
BASE_FEATURE(kEnergyEffectInNextbox, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksEnableFileHint, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksComposeboxJumpFix,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksComposeboxFork, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of a rounded clip-path for the composebox.
BASE_FEATURE(kContextualTasksRoundedClipPath, base::FEATURE_ENABLED_BY_DEFAULT);

// Hides the the 3-dot (overflow) menu when viewing an AI page in the side
// panel. The menu is still shown for lens flows.
BASE_FEATURE(kContextualTasksHideMenuOnAiPage,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksUnboundedMenu, base::FEATURE_DISABLED_BY_DEFAULT);

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

// When enabled, close tab actions can expand the side panel.
BASE_FEATURE(kContextualTasksCloseTabExpandsSidePanel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of APC comparison for webpages in the recontextualization
// flow.
BASE_FEATURE(kContextualTasksWebpageApcComparison,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables overriding side panel to show Bottom Sheet on demand.
BASE_FEATURE(kContextualTasksOverrideShowBottomSheetOnLargeScreen,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables prefetching of cookies for contextual tasks.
BASE_FEATURE(kContextualTasksCookiePrefetch, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimTriggeredThreadLinks, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksWindowTracking, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksUploadChunking, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksEnableSpatialModelToolbarLayout,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksRearchitecture, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksSidePanelRearchitecture,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksEnableStickyConversation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksClobberActiveTab,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualTasksNonBlockingUrlNavigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool GetIsContextualTasksNonBlockingUrlNavigationEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksNonBlockingUrlNavigation);
}

bool GetIsContextualTasksPdfCitationsEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksPdfCitations);
}

bool ShouldContextualTasksPrivateApiUseNoAnimation() {
  return base::FeatureList::IsEnabled(kContextualTasksPrivateApiNoAnimation);
}

bool GetIsContextualTasksSearchQueryEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSearchQuery);
}

bool GetIsContextualTasksLazyFetchClusterInfoEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksLazyFetchClusterInfo);
}

bool GetIsContextualTasksWindowTrackingEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksWindowTracking);
}

bool GetIsContextualTasksUploadChunkingEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksUploadChunking);
}

bool GetContextualTasksSpatialModelToolbarLayoutEnabled() {
  return base::FeatureList::IsEnabled(
      kContextualTasksEnableSpatialModelToolbarLayout);
}

bool IsStickyConversationEnabled() {
  if (g_sticky_conversation_enabled_override.has_value()) {
    return *g_sticky_conversation_enabled_override;
  }
  return base::FeatureList::IsEnabled(
             kContextualTasksEnableStickyConversation) &&
         ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
}

ScopedStickyConversationEnabledForTesting::
    ScopedStickyConversationEnabledForTesting(bool enabled) {
  CHECK(!g_sticky_conversation_enabled_override.has_value());
  g_sticky_conversation_enabled_override = enabled;
}

ScopedStickyConversationEnabledForTesting::
    ~ScopedStickyConversationEnabledForTesting() {
  g_sticky_conversation_enabled_override.reset();
}

const base::FeatureParam<OverflowMenuItems>::Option
    kContextualTasksSpatialModelToolbarLayoutOverflowItemsOptions[] = {
        {OverflowMenuItems::kAllItems, "all-items"},
        {OverflowMenuItems::kAllWithoutNewThread, "all-without-new-thread"},
};

const base::FeatureParam<OverflowMenuItems>
    kContextualTasksSpatialModelToolbarLayoutOverflowItems(
        &kContextualTasksEnableSpatialModelToolbarLayout,
        "overflow-items",
        OverflowMenuItems::kAllWithoutNewThread,
        &kContextualTasksSpatialModelToolbarLayoutOverflowItemsOptions);

bool GetContextualTasksSpatialModelToolbarLayoutNewThreadInOverflow() {
  return kContextualTasksSpatialModelToolbarLayoutOverflowItems.Get() ==
         OverflowMenuItems::kAllItems;
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

const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity(
    &kContextualTasksContext,
    "ContextualTasksContextOnlyUseTitles",
    false);

const base::FeatureParam<bool> kDeduplicateRelevantTabsByUrl(
    &kContextualTasksContext,
    "ContextualTasksContextDeduplicateByUrl",
    false);

const base::FeatureParam<double> kTabSelectionScoreThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextTabSelectionScoreThreshold", 0.4};

const base::FeatureParam<double> kContentVisibilityThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextContentVisibilityThreshold", 0.7};

const base::FeatureParam<int> kMaxConversationTurns{
    &kContextualTasksContext, "max_conversation_turns", 5};
const base::FeatureParam<int> kMaxTitlesPerThread{
    &kContextualTasksContext, "max_titles_per_thread", 25};

const base::FeatureParam<bool> kEnablePreviousTabFallback(
    &kContextualTasksContext,
    "ContextualTasksEnablePreviousTabFallback",
    true);

const base::FeatureParam<base::TimeDelta> kPreviousTabRecencyThreshold(
    &kContextualTasksContext,
    "ContextualTasksPreviousTabRecencyThreshold",
    base::Seconds(30));

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


const base::FeatureParam<SmartTabSharingIphFirstTimePromptOption>::Option
    kSmartTabSharingIphFirstTimePromptOptions[] = {
        {SmartTabSharingIphFirstTimePromptOption::kIphFirstTimePromptV1,
         "iphStsFirstTimePromptV1"},
        {SmartTabSharingIphFirstTimePromptOption::kIphFirstTimePromptV2,
         "iphStsFirstTimePromptV2"},
};
const base::FeatureParam<SmartTabSharingIphFirstTimePromptOption>
    kSmartTabSharingIphFirstTimePromptOption(
        &kContextualTasksContext,
        "ContextualTasksContextSmartTabSharingIphFirstTimePromptOption",
        SmartTabSharingIphFirstTimePromptOption::kIphFirstTimePromptV1,
        &kSmartTabSharingIphFirstTimePromptOptions);

const base::FeatureParam<SmartTabSharingIphDefaultOnOption>::Option
    kSmartTabSharingIphDefaultOnOptions[] = {
        {SmartTabSharingIphDefaultOnOption::kIphDefaultOnV1,
         "iphStsDefaultOnV1"},
        {SmartTabSharingIphDefaultOnOption::kIphDefaultOnV2,
         "iphStsDefaultOnV2"},
};
const base::FeatureParam<SmartTabSharingIphDefaultOnOption>
    kSmartTabSharingIphDefaultOnOption(
        &kContextualTasksContext,
        "ContextualTasksContextSmartTabSharingDefaultOnOption",
        SmartTabSharingIphDefaultOnOption::kIphDefaultOnV1,
        &kSmartTabSharingIphDefaultOnOptions);

const base::FeatureParam<SmartTabSharingIphTryItPromoOption>::Option
    kSmartTabSharingIphTryItPromoOptions[] = {
        {SmartTabSharingIphTryItPromoOption::kIphTryItPromoV1,
         "iphStsTryItPromoV1"},
        {SmartTabSharingIphTryItPromoOption::kIphTryItPromoV2,
         "iphStsTryItPromoV2"},
};
const base::FeatureParam<SmartTabSharingIphTryItPromoOption>
    kSmartTabSharingIphTryItPromoOption(
        &kContextualTasksContext,
        "ContextualTasksContextSmartTabSharingIphTryItPromoOption",
        SmartTabSharingIphTryItPromoOption::kIphTryItPromoV1,
        &kSmartTabSharingIphTryItPromoOptions);

const base::FeatureParam<SmartTabSharingMegaplusStringOption>::Option
    kSmartTabSharingMegaplusOptions[] = {
        {SmartTabSharingMegaplusStringOption::kMegaplusV1, "megaplusV1"},
        {SmartTabSharingMegaplusStringOption::kMegaplusV2, "megaplusV2"},
        {SmartTabSharingMegaplusStringOption::kMegaplusV3, "megaplusV3"},
};
const base::FeatureParam<SmartTabSharingMegaplusStringOption>
    kSmartTabSharingMegaplusStringOption(
        &kContextualTasksContext,
        "ContextualTasksContextSmartTabSharingMegaplusStringOption",
        SmartTabSharingMegaplusStringOption::kMegaplusV2,
        &kSmartTabSharingMegaplusOptions);
const base::FeatureParam<double> kContextualTasksContextLoggingSampleRate{
    &kContextualTasksContextLogging, "ContextualTasksContextLoggingSampleRate",
    1.0};

const base::FeatureParam<int> kMinQueryWords{
    &kContextualTasksContext, "ContextualTasksContextMinQueryWords", 3};

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
    {EntryPointOption::kToolbarEphemeralBranded, "toolbar-ephemeral-branded"}};

const base::FeatureParam<EntryPointOption> kShowEntryPoint(
    &kContextualTasksEphemeralBrandedEntryPoint,
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
// Version 2.5: Support for link click post messages and window.open calls from
//              AIM.
// Version 2.6: Add inverted quote and follow up injected input icons.
// Version 2.7: NLM hidden searchbox bug fixes.
const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix{
    &kContextualTasks, "contextual-tasks-user-agent-suffix", "Cobrowsing/2.7"};

const base::FeatureParam<std::string> kContextualTasksOAuthScopes{
    &kContextualTasksExtraOauthScopes, "ContextualTasksOAuthScopes", ""};

const base::FeatureParam<std::string> kContextualTasksHelpUrl(
    &kContextualTasks,
    "ContextualTasksHelpUrl",
    "https://support.google.com/websearch/");

const base::FeatureParam<std::string> kContextualTasksOverflowMenuHelpUrl(
    &kContextualTasks,
    "ContextualTasksOverflowMenuHelpUrl",
    "https://support.google.com/chrome/answer/17025061");

const base::FeatureParam<std::string> kContextualTasksTabHelpUrl(
    &kContextualTasks,
    "ContextualTasksTabHelpUrl",
    "https://support.google.com/chrome/answer/17025061");

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

const base::FeatureParam<int> kContextualTasksLensSearchTooltipDismissedCap(
    &kContextualTasksShowOnboardingTooltip,
    "ContextualTasksLensSearchTooltipDismissedCap", 1);

const base::FeatureParam<int>
    kContextualTasksLensSearchTooltipSessionImpressionCap(
        &kContextualTasksShowOnboardingTooltip,
        "ContextualTasksLensSearchTooltipSessionImpressionCap",
        1);

const base::FeatureParam<int> kContextualTasksAskGTooltipDismissedCap(
    &kContextualTasksShowOnboardingTooltip,
    "ContextualTasksAskGTooltipDismissedCap", 1);

const base::FeatureParam<int>
    kContextualTasksAskGTooltipSessionImpressionCap(
        &kContextualTasksShowOnboardingTooltip,
        "ContextualTasksAskGTooltipSessionImpressionCap",
        10);

const base::FeatureParam<int> kContextualTasksOnboardingTooltipImpressionDelay(
    &kContextualTasksShowOnboardingTooltip,
    "ContextualTasksOnboardingTooltipImpressionDelay",
    3000);

const base::FeatureParam<int> kContextualTasksNumSessionsBeforeRequestPinPromo(
    &kContextualTasks,
    "ContextualTasksPinPromoSessionDelay",
    2);

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
  if (base::FeatureList::IsEnabled(kContextualTasksBypassDismissedCap)) {
    return std::numeric_limits<int>::max();
  }
  return kContextualTasksOnboardingTooltipDismissedCap.Get();
}

int GetContextualTasksLensSearchTooltipDismissedCap() {
  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip)) {
    return 0;
  }
  if (base::FeatureList::IsEnabled(kContextualTasksBypassDismissedCap)) {
    return std::numeric_limits<int>::max();
  }
  return kContextualTasksLensSearchTooltipDismissedCap.Get();
}

int GetContextualTasksLensSearchTooltipSessionImpressionCap() {
  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip)) {
    return 0;
  }
  return kContextualTasksLensSearchTooltipSessionImpressionCap.Get();
}

int GetContextualTasksAskGTooltipDismissedCap() {
  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip)) {
    return 0;
  }
  if (base::FeatureList::IsEnabled(kContextualTasksBypassDismissedCap)) {
    return std::numeric_limits<int>::max();
  }
  return kContextualTasksAskGTooltipDismissedCap.Get();
}

int GetContextualTasksAskGTooltipSessionImpressionCap() {
  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip)) {
    return 0;
  }
  return kContextualTasksAskGTooltipSessionImpressionCap.Get();
}

int GetContextualTasksOnboardingTooltipImpressionDelay() {
  return kContextualTasksOnboardingTooltipImpressionDelay.Get();
}

int ContextualTasksInactiveSidePanelKeepInCacheMinutes() {
  if (!IsContextualTasksUIEnabled()) {
    return 0;
  }
  return kContextualTasksInactiveSidePanelKeepInCacheMinutes.Get();
}

bool IsContextualTasksPinButtonInToolbarEnabled() {
  return base::FeatureList::IsEnabled(kEnableContextualTasksPinButtonInToolbar);
}

int GetContextualTasksNumSessionsBeforeRequestPinPromo() {
  return kContextualTasksNumSessionsBeforeRequestPinPromo.Get();
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

std::optional<HostOverride> GetForcedEmbeddedPageHost() {
  std::optional<HostOverride> host_override =
      GetForcedEmbeddedPageHostOverride().has_value()
          ? GetForcedEmbeddedPageHostOverride()
          : HostOverride::FromString(
                kContextualTasksForcedEmbeddedPageHost.Get());

  if (!host_override.has_value()) {
    return std::nullopt;
  }

  // If there's a non-empty host, ensure that it is only ever going to a
  // google.com domain. If not, return std::nullopt.
  // LINT.IfChange(AllowedHosts)
  const std::string& host = host_override->host;
  if (!(base::EndsWith(host, ".google.com") ||
        base::EndsWith(host, ".googlers.com") || host == "google.com" ||
        host == "googlers.com")) {
    return std::nullopt;
  }
  // LINT.ThenChange(//chrome/browser/resources/contextual_tasks/internals/app.ts:AllowedHosts)

  return host_override;
}

void SetForcedEmbeddedPageHostOverride(
    std::optional<HostOverride> host_override) {
  GetForcedEmbeddedPageHostOverride() = std::move(host_override);
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
    100 * 1024 * 1024};

bool GetIsContextualTasksSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSuggestionsEnabled);
}


base::TimeDelta GetSmartTabSharingTabSelectionTimeout() {
  if (kSmartTabSharingTabSelectionTimeout.Get().is_positive()) {
    return kSmartTabSharingTabSelectionTimeout.Get();
  }
  return base::Milliseconds(300);
}


bool GetIsTabAutoSuggestionChipEnabled() {
  return kContextualTasksTabAutoSuggestionChipEnabled.Get();
}

bool GetEnableLensInContextualTasks() {
  return IsContextualTasksUIEnabled() && kEnableLensInContextualTasks.Get();
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

std::string GetContextualTasksOverflowMenuHelpUrl() {
  return kContextualTasksOverflowMenuHelpUrl.Get();
}

std::string GetContextualTasksTabHelpUrl() {
  return kContextualTasksTabHelpUrl.Get();
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
  return base::FeatureList::IsEnabled(kContextualTasksCookiePrefetch);
}

bool ShouldEnableLockAndUnlockInputCapability() {
  return base::FeatureList::IsEnabled(kContextualTasks) &&
         kContextualTasksLockAndUnlockInputCapability.Get();
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

bool IsContextualTasksRearchitectureEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksRearchitecture);
}

bool IsContextualTasksSidePanelRearchitectureEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSidePanelRearchitecture);
}

bool IsContextualTasksClobberActiveTabEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksClobberActiveTab);
}

const base::FeatureParam<std::string> kContextualTasksSearchCapabilitiesVersion{
    &kContextualTasksRearchitecture,
    "contextual-tasks-search-capabilities-version",
    kContextualTasksSearchCapabilitiesDefaultVersion};

std::string GetContextualTasksSearchCapabilitiesVersion() {
  return kContextualTasksSearchCapabilitiesVersion.Get();
}

bool IsContextualTasksUnboundedMenuEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksUnboundedMenu) ||
         IsContextualTasksSidePanelRearchitectureEnabled();
}

bool IsContextualTasksUIEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSidePanel) ||
         base::FeatureList::IsEnabled(kContextualTasks) ||
         base::FeatureList::IsEnabled(kContextualTasksRearchitecture);
}

namespace flag_descriptions {

const char kContextualTasksPrivateApiNoAnimationName[] =
    "Contextual Tasks Private API No Animation";
const char kContextualTasksPrivateApiNoAnimationDescription[] =
    "Disable animation when opening Contextual Tasks side panel from the "
    "private API.";

const char kContextualTasksName[] = "Contextual Tasks";
const char kContextualTasksDescription[] =
    "Enable the contextual tasks feature.";

const char kContextualTasksSidePanelName[] = "Contextual Tasks Side Panel";
const char kContextualTasksSidePanelDescription[] =
    "Enable the contextual tasks side panel infrastructure without enabling "
    "full Contextual Tasks capabilities.";

const char kContextualTasksContextName[] = "Contextual Tasks Context";
const char kContextualTasksContextDescription[] =
    "Enables relevant context determination for contextual tasks.";

const char kContextualTasksSearchQueryName[] = "Contextual Tasks Search Query";
const char kContextualTasksSearchQueryDescription[] =
    "Enables forwarding the search query parameter 'q' in contextual tasks.";

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

const char kContextualTasksCloseTabExpandsSidePanelName[] =
    "Contextual Tasks Close Tab Expands Side Panel";
const char kContextualTasksCloseTabExpandsSidePanelDescription[] =
    "Enables expanding the contextual tasks side panel on close tab actions.";

const char kContextualTasksOverrideShowBottomSheetOnLargeScreenName[] =
    "Override Show Bottom Sheet On Large Screen for Contextual Tasks";
const char kContextualTasksOverrideShowBottomSheetOnLargeScreenDescription[] =
    "Enables overriding side panel to show Bottom Sheet on large screens for "
    "contextual tasks.";

const char kContextualTasksCookiePrefetchName[] =
    "Contextual Tasks Cookie Prefetch";
const char kContextualTasksCookiePrefetchDescription[] =
    "Enables prefetching of cookies for contextual tasks.";

const char kEnableContextualTasksPinButtonInToolbarName[] =
    "Contextual Tasks Pin Button In Toolbar";
const char kEnableContextualTasksPinButtonInToolbarDescription[] =
    "Enables the pin button in the toolbar for contextual tasks.";

const char kContextualTasksHideMenuOnAiPageName[] =
    "Contextual Tasks Hide Menu On AI Page";
const char kContextualTasksHideMenuOnAiPageDescription[] =
    "Hides the 3-dot (overflow) menu when viewing an AI page in the side "
    "panel. The menu is still shown for lens flows.";

const char kContextualTasksEnableSpatialModelToolbarLayoutName[] =
    "Contextual Tasks Enable Spatial Model Toolbar Layout";
const char kContextualTasksEnableSpatialModelToolbarLayoutDescription[] =
    "Enables the spatial model toolbar layout for contextual tasks.";

const char kContextualTasksRearchitectureName[] =
    "Contextual Tasks Rearchitecture";
const char kContextualTasksRearchitectureDescription[] =
    "Enables composebox embedded in AIM main frame, new auth,"
    " and new side panel and ghost loader for contextual tasks.";

const char kContextualTasksEphemeralBrandedEntryPointName[] =
    "Contextual Tasks Ephemeral Branded Entry Point";
const char kContextualTasksEphemeralBrandedEntryPointDescription[] =
    "Enables the ephemeral branded entry point for contextual tasks.";
const char kContextualTasksSidePanelRearchitectureName[] =
    "Contextual Tasks Side Panel Rearchitecture";
const char kContextualTasksSidePanelRearchitectureDescription[] =
    "Enables the side panel rearchitecture for contextual tasks.";

const char kContextualTasksClobberActiveTabName[] =
    "Contextual Tasks Clobber Active Tab";
const char kContextualTasksClobberActiveTabDescription[] =
    "Enables clicking links in the contextual tasks side panel to clobber the "
    "active tab instead of opening in a new tab.";

const char kContextualTasksBypassDismissedCapName[] =
    "Contextual Tasks Bypass Dismissed Cap";
const char kContextualTasksBypassDismissedCapDescription[] =
    "Debugging flag that bypasses the dismissal count limit for contextual "
    "tasks tooltips, allowing them to be shown even after the user has "
    "dismissed them.";

const char kEphemeralPinningVisibleWhenPermanentlyPinnedName[] =
    "Contextual Tasks Ephemeral Pinning Visible When Permanently Pinned";
const char kEphemeralPinningVisibleWhenPermanentlyPinnedDescription[] =
    "Keeps the ephemeral contextual tasks button visible even when the "
    "permanent button is pinned in the toolbar.";

}  // namespace flag_descriptions

}  // namespace contextual_tasks
