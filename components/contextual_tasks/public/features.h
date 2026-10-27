// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/contextual_tasks/public/host_override.h"

namespace contextual_tasks {

BASE_DECLARE_FEATURE(kContextualTasks);
BASE_DECLARE_FEATURE(kContextualTasksPrivateApiNoAnimation);
BASE_DECLARE_FEATURE(kContextualTasksSidePanel);
BASE_DECLARE_FEATURE(kContextualTasksSidePanelDefaultRight);
BASE_DECLARE_FEATURE(kContextualTasksEphemeralBrandedEntryPoint);
BASE_DECLARE_FEATURE(kContextualTasksUpdatedEntryPoints);
BASE_DECLARE_FEATURE(kContextualTasksExtraOauthScopes);
BASE_DECLARE_FEATURE(kContextualTasksDriveOAuthScope);
BASE_DECLARE_FEATURE(kEnableContextualTasksPinButtonInToolbar);
BASE_DECLARE_FEATURE(kEnableContextualTasksPinButtonOnSrp);
BASE_DECLARE_FEATURE(kContextualTasksPinningV2);
BASE_DECLARE_FEATURE(kContextualTasksPinnedButtonActiveIndicator);
BASE_DECLARE_FEATURE(kContextualTasksUnifyPinButtonLogos);
BASE_DECLARE_FEATURE(kEphemeralPinningVisibleWhenPermanentlyPinned);
BASE_DECLARE_FEATURE(kContextualTasksEphemeralButtonContextMenu);
BASE_DECLARE_FEATURE(kContextualTasksEphemeralButtonAutoDismiss);
BASE_DECLARE_FEATURE(kContextualTasksContext);
BASE_DECLARE_FEATURE(kContextualTasksSearchQuery);
BASE_DECLARE_FEATURE(kContextualTasksContextMultiTurnTabRelevance);
BASE_DECLARE_FEATURE(
    kContextualTasksContextSmartTabSharingDefaultOnAvailability);
BASE_DECLARE_FEATURE(kContextualTasksContextLibrary);
BASE_DECLARE_FEATURE(kContextualTasksContextLogging);
BASE_DECLARE_FEATURE(kContextualTasksShowOnboardingTooltip);

// Bypasses the dismissed cap for contextual tasks tooltips.
BASE_DECLARE_FEATURE(kContextualTasksBypassDismissedCap);

// Enables prefetching of cookies for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksCookiePrefetch);

// Overrides the value of EntryPointEligibilitymanager::IsEligible to true.
BASE_DECLARE_FEATURE(kContextualTasksForceEntryPointEligibility);

// Enables context menu settings for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksContextMenu);

// Enables context menu settings for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksSuggestionsEnabled);

// Force the application locale to US and the gl query parameter to us.
BASE_DECLARE_FEATURE(kContextualTasksForceCountryCodeUS);

// Remove tasks that have no tabs or threads associated with them on tab
// disassociation.
BASE_DECLARE_FEATURE(
    kContextualTasksRemoveTasksWithoutThreadsOrTabAssociations);

// Enables use of silk api to notify zero state rendered instead of the url
// param.
BASE_DECLARE_FEATURE(kEnableNotifyZeroStateRenderedCapability);

// If enabled, adds the Sec-CH-UA-Full-Version-List header to all network
// requests initiated from within an embedded Co-Browse <webview>.
BASE_DECLARE_FEATURE(kContextualTasksSendFullVersionListEnabled);

// If enabled, AIM will send the ContextualInputUploadType enum on
// ContextualInputs.
BASE_DECLARE_FEATURE(kContextualTasksSendContextualInputUploadType);

// When contextual tasks is disabled and this flag is enabled, intecept the
// contextual tasks URL and redirect to aim URL.
BASE_DECLARE_FEATURE(kContextualTasksUrlRedirectToAimUrl);

// If enabled, animates the caret.
BASE_DECLARE_FEATURE(kContextualTasksAnimatedCaret);

// Enables energy effect in Nextbox.
BASE_DECLARE_FEATURE(kEnergyEffectInNextbox);

// Fixes the composebox jump.
BASE_DECLARE_FEATURE(kContextualTasksComposeboxJumpFix);

// Enables the use of a rounded clip-path for the composebox.
BASE_DECLARE_FEATURE(kContextualTasksRoundedClipPath);

// Hides the the 3-dot (overflow) menu when viewing an ai page in the side
// panel. The menu is still shown for lens flows.
BASE_DECLARE_FEATURE(kContextualTasksHideMenuOnAiPage);

// Enables painting dropdown menus outside WebUI boundaries.
BASE_DECLARE_FEATURE(kContextualTasksUnboundedMenu);

// Enables hiding the close button when in vertical tabs or immersive mode.
BASE_DECLARE_FEATURE(kContextualTasksHideCloseButtonInVerticalTabs);

// Enables intercepting YouTube links with timestamps to seek video instead of
// navigating.
BASE_DECLARE_FEATURE(kContextualTasksVideoCitations);

// Enables intercepting PDF links with page numbers to scroll to page instead of
// navigating.
BASE_DECLARE_FEATURE(kContextualTasksPdfCitations);

// When enabled, the back button can expand the side panel.
BASE_DECLARE_FEATURE(kContextualTasksBackButtonExpandsSidePanel);

// When enabled, close tab actions can expand the side panel.
BASE_DECLARE_FEATURE(kContextualTasksCloseTabExpandsSidePanel);

// Enables lazy fetching of cluster info for multimodal queries.
BASE_DECLARE_FEATURE(kContextualTasksLazyFetchClusterInfo);

// Enables the use of APC comparison for webpages in the recontextualization
BASE_DECLARE_FEATURE(kContextualTasksWebpageApcComparison);

// Enables overriding side panel to show Bottom Sheet on demand.
BASE_DECLARE_FEATURE(kContextualTasksOverrideShowBottomSheetOnLargeScreen);

// When enabled, AIM must send the browser a message to initiate the cobrowse
// experience for link clicks.
BASE_DECLARE_FEATURE(kAimTriggeredThreadLinks);

// Enables window tracking for Contextual Tasks.
BASE_DECLARE_FEATURE(kContextualTasksWindowTracking);

// Enables upload chunking for Contextual Tasks.
BASE_DECLARE_FEATURE(kContextualTasksUploadChunking);

// Enables the in-panel WebUI voice search UI on large-screen Android (tablet
// and desktop form factors), instead of delegating to the platform voice
// recognition activity. Has no effect on Android phones, or off Android, which
// always uses the WebUI voice search UI.
BASE_DECLARE_FEATURE(kContextualTasksWebUiVoiceSearchDesktopAndroid);

// Enables composebox embedded in AIM main frame, new auth, and
// new side panel and ghost loader for contextual tasks.
BASE_DECLARE_FEATURE(kContextualTasksRearchitecture);

// Enables the side panel changes as part of the rearchitecture.
BASE_DECLARE_FEATURE(kContextualTasksSidePanelRearchitecture);

// Enables sticky conversation UI that follows the user around.
BASE_DECLARE_FEATURE(kContextualTasksEnableStickyConversation);

// Enables clicking links in contextual tasks side panel to clobber the active
// tab next to the side panel instead of opening a new tab.
BASE_DECLARE_FEATURE(kContextualTasksClobberActiveTab);

// Enables tab-specific side panel state and navigation improvements across
// standard Chrome tab actions.
BASE_DECLARE_FEATURE(kContextualTasksSidePanelTabState);

// Controls whether clicking links in the contextual tasks side panel always
// opens a new tab instead of focusing an existing affiliated tab (except for
// citations on the active tab).
extern const base::FeatureParam<bool> kOpenNewTabOnSidePanelLinkClick;

// Returns whether clicking links in the contextual tasks side panel should
// always open a new tab instead of focusing an existing affiliated tab.
bool ShouldOpenNewTabOnSidePanelLinkClick();

// Controls whether closing the contextual tasks side panel or changing its
// thread only applies to the tab where the user took the action, instead of
// all tabs affiliated with the same task.
extern const base::FeatureParam<bool> kScopeSidePanelStateToTab;

// Returns whether closing the contextual tasks side panel or changing its
// thread should only apply to the tab where the user took the action.
bool ShouldScopeSidePanelStateToTab();

// Controls whether reopening a recently closed tab restores the contextual
// tasks side panel and its thread if the panel was open on the tab.
extern const base::FeatureParam<bool> kRestoreSidePanelOnTabRestore;

// Returns whether reopening a recently closed tab should restore the
// contextual tasks side panel and its thread if the panel was open on the tab.
bool ShouldRestoreSidePanelOnTabRestore();

// Enables emitting attached tab context tokens in OnSuggestInputsChanged for
// tab-based query suggestions.
BASE_DECLARE_FEATURE(kContextualTasksTabBasedSuggestions);

// When enabled, allows AIM search URLs to be created and navigated immediately
// upon starting contextual input uploads without waiting for background
// network uploads to complete.
BASE_DECLARE_FEATURE(kContextualTasksNonBlockingUrlNavigation);

// Returns whether non-blocking AIM URL navigation is enabled.
bool GetIsContextualTasksNonBlockingUrlNavigationEnabled();

// Allows signed-out users to use side panel cobrowse in Desktop Android.
BASE_DECLARE_FEATURE(kAllowSignedOutUserInDesktopAndroid);

// Returns whether signed-out users are allowed to use side panel cobrowse in
// Desktop Android.
bool IsAllowSignedOutUserInDesktopAndroidEnabled();

// When enabled, tracks cross-tab ABA search journeys (Tab A -> Search in Tab B
// -> Return to Tab A) to trigger an in-product help promo for Contextual Tasks.
BASE_DECLARE_FEATURE(kABASearchJourneys);

extern const base::FeatureParam<base::TimeDelta> kABASearchJourneysTtl;
extern const base::FeatureParam<size_t> kABASearchJourneysMaxRingBufferSize;
extern const base::FeatureParam<size_t> kABASearchJourneysMinQueryMatchLength;
extern const base::FeatureParam<bool> kABASearchJourneysAllowIntermediateTabs;

bool IsABASearchJourneysEnabled();

base::TimeDelta GetABASearchJourneysTtl();
size_t GetABASearchJourneysMaxRingBufferSize();
size_t GetABASearchJourneysMinQueryMatchLength();

// Returns whether visiting other tabs between the search tab and the return to
// the source tab (A -> B -> C -> A) keeps the journey. Off by default, so only
// A -> B -> A counts.
bool ShouldAllowABASearchJourneysIntermediateTabs();

BASE_DECLARE_FEATURE(kContextualTasksEnableSpatialModelToolbarLayout);

BASE_DECLARE_FEATURE(kContextualSearchContextualCuesHandleEdu);
BASE_DECLARE_FEATURE(kContextualSearchContextualCuesHandleShopping);

enum class OverflowMenuItems {
  kAllItems,
  kAllWithoutNewThread,
};

extern const base::FeatureParam<OverflowMenuItems>
    kContextualTasksSpatialModelToolbarLayoutOverflowItems;

bool GetIsContextualTasksPdfCitationsEnabled();
bool ShouldContextualTasksPrivateApiUseNoAnimation();
bool GetIsContextualTasksSearchQueryEnabled();

bool GetIsContextualTasksLazyFetchClusterInfoEnabled();

bool GetIsContextualTasksWindowTrackingEnabled();

bool GetIsContextualTasksUploadChunkingEnabled();

bool GetIsContextualTasksWebUiVoiceSearchDesktopAndroidEnabled();

bool GetContextualTasksSpatialModelToolbarLayoutEnabled();

bool GetContextualTasksSpatialModelToolbarLayoutNewThreadInOverflow();

// Returns whether the Contextual Tasks side panel container, coordinator,
// and supporting response services should be initialized.
// Returns true if full Contextual Tasks (`kContextualTasks`),
// infrastructure-only mode (`kContextualTasksSidePanel`), or the Contextual
// Tasks rearchitecture (`kContextualTasksRearchitecture` /
// `kContextualTasksSidePanelRearchitecture`) is enabled.
bool IsContextualTasksUIEnabled();

bool IsStickyConversationEnabled();

// Test utility for overriding conditions for sticky conversation.
class ScopedStickyConversationEnabledForTesting {
 public:
  explicit ScopedStickyConversationEnabledForTesting(bool enabled);
  ~ScopedStickyConversationEnabledForTesting();
};

// Enum denoting which entry point can show when enabled.
enum class EntryPointOption {
  kNoEntryPoint,
  kToolbarEphemeralBranded,
};

// Enum of expand button UI option
enum class ExpandButtonOption {
  kSidePanelExpandButton,
  kToolbarCloseButton,
};

// Whether to only consider titles for similarity.
extern const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity;
// Whether to deduplicate relevant tabs by URL.
extern const base::FeatureParam<bool> kDeduplicateRelevantTabsByUrl;
// Minimum score to consider a tab relevant.
extern const base::FeatureParam<double> kTabSelectionScoreThreshold;
// Minimum score required for a tab to be considered visible.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

// Maximum number of conversation turns to embed and pass to the model.
extern const base::FeatureParam<int> kMaxConversationTurns;
// Maximum number of shared tab titles from context library to embed and pass.
extern const base::FeatureParam<int> kMaxTitlesPerThread;

// Whether to use the immediately previous visited tab as the active tab signal
// fallback.
extern const base::FeatureParam<bool> kEnablePreviousTabFallback;
// Recency threshold for using the previous visited tab as active tab signal.
extern const base::FeatureParam<base::TimeDelta> kPreviousTabRecencyThreshold;

// Whether Smart Tab Sharing is enabled for the ContextualTasksContext feature.
extern const base::FeatureParam<bool> kContextualTasksContextSmartTabSharing;

// Whether Smart Tab Sharing should toggle off after query submission.
extern const base::FeatureParam<bool>
    kContextualTasksContextToggleOffAfterSubmit;

// Option for smart tab sharing IPH first time prompt.
enum class SmartTabSharingIphFirstTimePromptOption {
  kIphFirstTimePromptV1,
  kIphFirstTimePromptV2,
};
extern const base::FeatureParam<SmartTabSharingIphFirstTimePromptOption>
    kSmartTabSharingIphFirstTimePromptOption;

// Option for smart tab sharing IPH default on variants.
enum class SmartTabSharingIphDefaultOnOption {
  kIphDefaultOnV1,
  kIphDefaultOnV2,
};
extern const base::FeatureParam<SmartTabSharingIphDefaultOnOption>
    kSmartTabSharingIphDefaultOnOption;

// Option for smart tab sharing IPH try it promo variants.
enum class SmartTabSharingIphTryItPromoOption {
  kIphTryItPromoV1,
  kIphTryItPromoV2,
};
extern const base::FeatureParam<SmartTabSharingIphTryItPromoOption>
    kSmartTabSharingIphTryItPromoOption;

// Option for smart tab sharing megaplus string.
enum class SmartTabSharingMegaplusStringOption {
  kMegaplusV1,
  kMegaplusV2,
  kMegaplusV3,
};
extern const base::FeatureParam<SmartTabSharingMegaplusStringOption>
    kSmartTabSharingMegaplusStringOption;

// Task string to use for formatting the query embedding.
extern const base::FeatureParam<std::string> kQueryEmbeddingTask;

// The sample rate for logging contextual tasks context quality.
extern const base::FeatureParam<double>
    kContextualTasksContextLoggingSampleRate;

// The minimum number of words required in a query for contextual tasks context
// tab relevancy selection.
extern const base::FeatureParam<int> kMinQueryWords;

// Controls whether we set the upload type in CreateSearchUrl.
extern const base::FeatureParam<bool> kSendContextualInputUploadTypeInSearchUrl;

// Controls whether we set the upload type in CreateClientToAimRequest.
extern const base::FeatureParam<bool>
    kSendContextualInputUploadTypeInAimRequest;

// Controls whether the contextual task page action should show
extern const base::FeatureParam<EntryPointOption, true> kShowEntryPoint;

// Returns whether the right hand contextual tasks ephemeral button is enabled
// when the side panel is right-aligned.
extern bool GetEnableRightHandContextualTasksEphemeralButton();

// Controls whether the contextual tasks ephemeral button is placed as a regular
// circular button with drop shadow next to the battery saver button.
BASE_DECLARE_FEATURE_PARAM(bool,
                           kEnableCircularEphemeralButtonNextToBatterySaver);

// Controls whether the ephemeral contextual tasks toolbar button context menu
// displays "Close side panel" instead of "Hide side panel".
extern const base::FeatureParam<bool>
    kContextualTasksEphemeralButtonContextMenuCloseSidePanel;

bool ShouldUseCloseSidePanelForEphemeralButtonContextMenu();

// Duration in minutes after closing the contextual tasks side panel before the
// ephemeral toolbar button is automatically dismissed and its task
// disassociated. Defaults to 120 minutes (2 hours).
extern const base::FeatureParam<int>
    kContextualTasksEphemeralButtonAutoDismissTimeMinutes;

bool IsContextualTasksEphemeralButtonAutoDismissEnabled();
int GetContextualTasksEphemeralButtonAutoDismissTimeMinutes();

// Logo option for unifying the SIC (Contextual Tasks) toolbar and ephemeral
// pin buttons.
enum class UnifyPinButtonLogoOption {
  kSpark,
  kSuperG,
  kGraySuperG,
};

extern const base::FeatureParam<UnifyPinButtonLogoOption>
    kContextualTasksUnifyPinButtonLogoOption;

// Returns whether both the toolbar and ephemeral pin buttons should be
// unified to use the Spark logo.
bool ShouldUseSparkIconForPinButtons();

// Returns whether both the toolbar and ephemeral pin buttons should be
// unified to use the Super G logo.
bool ShouldUseSuperGIconForPinButtons();

// Returns whether both the toolbar and ephemeral pin buttons should be
// unified to use the gray (monochrome) Super G logo.
bool ShouldUseGraySuperGIconForPinButtons();

// UI Options to expand the contextual tasks side panel to tab.
extern const base::FeatureParam<ExpandButtonOption, true> kExpandButtonOptions;

// Controls whether the "Ask Google..." context menu item is shown.
extern const base::FeatureParam<bool> kContextualTasksContextMenuShowAskGoogle;

// Controls whether "Ask Google..." entrypoints are routed to the omnibox.
extern const base::FeatureParam<bool>
    kContextualTasksContextMenuRouteAskGoogleToOmnibox;

// Controls whether the contextual tasks context menu items are grouped into a
// submenu.
extern const base::FeatureParam<bool> kContextualTasksContextMenuSubmenu;

// Whether the context menu is enabled for Nextbox.
extern bool GetIsContextualTasksNextboxContextMenuEnabled();

// The file types that can be attached to a Nextbox as images.
extern const base::FeatureParam<std::string>
    kContextualTasksNextboxImageFileTypes;

// The file types that can be attached to a Nextbox as attachments.
extern const base::FeatureParam<std::string>
    kContextualTasksNextboxAttachmentFileTypes;

// The maximum size of a file that can be attached to a Nextbox.
extern const base::FeatureParam<int> kContextualTasksNextboxMaxFileSize;

// The number of sessions after onboarding before triggering the pinning IPH.
extern const base::FeatureParam<int>
    kContextualTasksNumSessionsBeforeRequestPinPromo;

// The user agent suffix to use for requests from the contextual tasks UI.
extern const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix;

// Extra OAuth scopes separated by commas for contextual tasks.
extern const base::FeatureParam<std::string> kContextualTasksOAuthScopes;

// The URL for the help center article from the toolbar.
extern const base::FeatureParam<std::string> kContextualTasksHelpUrl;

// The URL for the help center article linked from the onboarding tooltip.
extern const base::FeatureParam<std::string>
    kContextualTasksOnboardingTooltipHelpUrl;

// The URL for the help center article linked from the overflow menu.
extern const base::FeatureParam<std::string>
    kContextualTasksOverflowMenuHelpUrl;

// Enables suggestions rendered on contextual tasks side, instead of from AIM
// webpage.
extern const base::FeatureParam<bool>
    kContextualTasksEnableNativeZeroStateSuggestions;

// The maximum number of times the onboarding tooltip can be shown to the user
// in a single session before it no longer shows up.
extern int GetContextualTasksShowOnboardingTooltipSessionImpressionCap();

// The maximum number of times the onboarding tooltip can be dismissed by the
// user before it no longer shows up.
extern int GetContextualTasksOnboardingTooltipDismissedCap();

// The maximum number of times the AskG tooltip can be dismissed by the
// user before it no longer shows up.
extern int GetContextualTasksAskGTooltipDismissedCap();

// The maximum number of times the AskG tooltip can be shown to the user
// in a single session before it no longer shows up.
extern int GetContextualTasksAskGTooltipSessionImpressionCap();

// The delay in milliseconds before the onboarding tooltip is considered shown.
extern int GetContextualTasksOnboardingTooltipImpressionDelay();

// The number of seconds inactive side panel WebContents should keep in cache.
// Expired side panel WebContents will be destroyed.
extern int ContextualTasksInactiveSidePanelKeepInCacheMinutes();

// Returns if the protected page error is enabled.
extern bool GetIsProtectedPageErrorEnabled();

// Returns if the ghost loader is enabled.
extern bool GetIsGhostLoaderEnabled();

// Returns if basic mode should be forced when the thread history is opened
// before the handshake is complete.
extern bool ShouldForceBasicModeIfOpeningThreadHistory();

// Returns the base URL for the AI page.
extern std::string GetContextualTasksAiPageUrl();

// Returns the base URL for a Gemini thread.
extern std::string GetContextualTasksGeminiBaseUrl();

// Returns scheme component of the "display url" associated with the contextual
// tasks page.
extern std::string GetContextualTasksDisplayUrlScheme();

// Returns host component of the "display url" associated with the contextual
// tasks page.
extern std::string GetContextualTasksDisplayUrlHost();

// Returns path component of the "display url" associated with the contextual
// tasks page.
extern std::string GetContextualTasksDisplayUrlPath();

// Returns whether to show the expanded security chip in the location bar for
// the contextual tasks page.
extern bool ShouldShowExpandedSecurityChip();

// Returns the host that all URLs loaded in the embedded page in the Contextual
// Tasks WebUi should be routed to.
extern std::optional<HostOverride> GetForcedEmbeddedPageHost();

// Allows overriding the embedded page host at runtime for debugging.
extern void SetForcedEmbeddedPageHostOverride(
    std::optional<HostOverride> host_override);

// Returns the domains for the sign in page.
extern std::vector<std::string> GetContextualTasksSignInDomains();

// Whether the suggestions are enabled for Nextbox.
extern bool GetIsContextualTasksSuggestionsEnabled();

// Returns the timeout for smart tab sharing tab selection.
extern base::TimeDelta GetSmartTabSharingTabSelectionTimeout();

// Returns whether Smart Tab Sharing should toggle off after query submission.
extern bool ShouldToggleOffAfterSubmit();

// Enables tab auto-chip for contextual tasks. When disabled, no suggested
// chips will be shown in the composebox automatically.
extern bool GetIsTabAutoSuggestionChipEnabled();

// Returns whether Lens is enabled in contextual tasks. When this is enabled,
// Lens entry points will open results in the contextual tasks panels.
extern bool GetEnableLensInContextualTasks();

// Returns whether we should force the gsc=2 param to be added. This is used as
// a temporary workaround since the server is not yet ready to adapt the side
// panel UI unless the gsc=2 param is set.
extern bool ShouldForceGscInTabMode();

// Returns whether the country code should be forced to US.
extern bool ShouldForceCountryCodeUS();

// Returns the user agent suffix to use for requests.
extern std::string GetContextualTasksUserAgentSuffix();

// Returns the URL parameter name to check for NLM mode.
extern std::string GetContextualTasksNlmUrlParam();
extern bool IsCustomNlmUiEnabled();

// Whether the contextual tasks context quality should be logged.
extern bool ShouldLogContextualTasksContextQuality();

// Returns the help URL for the onboarding tooltip.
extern std::string GetContextualTasksOnboardingTooltipHelpUrl();

// Returns the help URL for the help center article from the toolbar.
extern std::string GetContextualTasksHelpUrl();

// Returns the help URL for the overflow menu help center article.
extern std::string GetContextualTasksOverflowMenuHelpUrl();

// Returns the help URL for the tab help center article.
extern std::string GetContextualTasksTabHelpUrl();

// Returns whether smart compose is enabled for Contextual Tasks.
extern bool GetEnableContextualTasksSmartCompose();

// Returns whether native (cobrowsing instead of AIM webpage)
// zero state suggestions are enabled for Contextual Tasks.
extern bool GetEnableNativeZeroStateSuggestions();

// Returns whether basic mode should be enabled.
extern bool GetIsBasicModeEnabled();

// Returns whether the z-order of the composebox should be changed in basic
// mode.
extern bool ShouldEnableBasicModeZOrder();

// Returns whether the cookie sync should be enabled.
extern bool ShouldEnableCookieSync();

// Returns whether the cookie prefetch should be enabled.
extern bool ShouldEnableCookiePrefetch();

// Returns whether the input plate can be locked and unlocked by a message
// from AIM.
extern bool ShouldEnableLockAndUnlockInputCapability();

// Returns whether the file hint is enabled in the composebox.
extern bool GetEnableFileHint();

// Returns whether the composebox jump fix is enabled.
extern bool GetEnableComposeboxJumpFix();

// Returns the UI option to expand contextual tasks side panel to tab.
extern ExpandButtonOption GetExpandButtonOption();

// Returns whether the rounded clip-path is enabled.
extern bool IsRoundedClipPathEnabled();

// Returns whether the pin button in toolbar is enabled.
extern bool IsContextualTasksPinButtonInToolbarEnabled();

// Returns whether Pinning V2 behavior (consolidating the pinning and ephemeral
// toolbar buttons for the right-hand Search in Chrome side panel) is enabled.
extern bool IsContextualTasksPinningV2Enabled();

// Returns whether the permanent pinned contextual tasks toolbar button should
// show an active indicator when the active tab has an ongoing thread/session.
extern bool IsContextualTasksPinnedButtonActiveIndicatorEnabled();

// Controls whether the permanent pinned contextual tasks toolbar button context
// menu displays a "Close/Hide side panel" item when the active tab has an
// ongoing thread/session.
extern const base::FeatureParam<bool>
    kContextualTasksPinnedButtonContextMenuCloseSidePanel;

// Returns whether the permanent pinned contextual tasks toolbar button context
// menu should show a "Close/Hide side panel" item when active.
extern bool ShouldShowCloseSidePanelInPinnedButtonContextMenu();

// Whether the active status indicator on the pin button should use a blue
// highlight for the circle when the side panel is closed instead of a blue icon
// and underline pill.
extern const base::FeatureParam<bool>
    kContextualTasksPinnedButtonActiveIndicatorCircleHighlight;
extern bool IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled();
// Returns whether the pin button in the side panel header is enabled on SRP.
extern bool IsContextualTasksPinButtonOnSrpEnabled();

// Returns the number of sessions after onboarding before triggering the pinning
// IPH.
extern int GetContextualTasksNumSessionsBeforeRequestPinPromo();

// Returns whether the webpage APC comparison is enabled.
extern bool GetIsWebpageApcComparisonEnabled();

extern bool IsContextualTasksRearchitectureEnabled();
extern bool IsContextualTasksSidePanelRearchitectureEnabled();
extern bool IsContextualTasksClobberActiveTabEnabled();
extern bool IsContextualTasksTabBasedSuggestionsEnabled();
extern bool IsContextualTasksUnboundedMenuEnabled();

inline constexpr char kContextualTasksSearchCapabilitiesHeaderName[] =
    "Chrome-Search-Capabilities-Version";
inline constexpr char kContextualTasksSearchCapabilitiesDefaultVersion[] = "1";

extern const base::FeatureParam<std::string>
    kContextualTasksSearchCapabilitiesVersion;
std::string GetContextualTasksSearchCapabilitiesVersion();

namespace flag_descriptions {

extern const char kContextualTasksPrivateApiNoAnimationName[];
extern const char kContextualTasksPrivateApiNoAnimationDescription[];
extern const char kContextualTasksName[];
extern const char kContextualTasksDescription[];
extern const char kContextualTasksSidePanelName[];
extern const char kContextualTasksSidePanelDescription[];
extern const char kContextualTasksSidePanelDefaultRightName[];
extern const char kContextualTasksSidePanelDefaultRightDescription[];
extern const char kContextualTasksContextLibraryName[];
extern const char kContextualTasksContextLibraryDescription[];
extern const char kContextualTasksContextName[];
extern const char kContextualTasksContextDescription[];
extern const char kContextualTasksContextMultiTurnTabRelevanceName[];
extern const char kContextualTasksContextMultiTurnTabRelevanceDescription[];
extern const char kContextualTasksSearchQueryName[];
extern const char kContextualTasksSearchQueryDescription[];
extern const char kContextualTasksSuggestionsEnabledName[];
extern const char kContextualTasksSuggestionsEnabledDescription[];
extern const char kContextualTasksBackButtonExpandsSidePanelName[];
extern const char kContextualTasksBackButtonExpandsSidePanelDescription[];
extern const char kContextualTasksCloseTabExpandsSidePanelName[];
extern const char kContextualTasksCloseTabExpandsSidePanelDescription[];
extern const char kContextualTasksOverrideShowBottomSheetOnLargeScreenName[];
extern const char
    kContextualTasksOverrideShowBottomSheetOnLargeScreenDescription[];
extern const char kContextualTasksCookiePrefetchName[];
extern const char kContextualTasksCookiePrefetchDescription[];
extern const char kEnableContextualTasksPinButtonInToolbarName[];
extern const char kEnableContextualTasksPinButtonInToolbarDescription[];
extern const char kEnableContextualTasksPinButtonOnSrpName[];
extern const char kEnableContextualTasksPinButtonOnSrpDescription[];
extern const char kContextualTasksPinningV2Name[];
extern const char kContextualTasksPinningV2Description[];
extern const char kContextualTasksPinnedButtonActiveIndicatorName[];
extern const char kContextualTasksPinnedButtonActiveIndicatorDescription[];
extern const char kContextualTasksHideMenuOnAiPageName[];
extern const char kContextualTasksHideMenuOnAiPageDescription[];
extern const char kContextualTasksEnableSpatialModelToolbarLayoutName[];
extern const char kContextualTasksEnableSpatialModelToolbarLayoutDescription[];
extern const char kContextualTasksRearchitectureName[];
extern const char kContextualTasksRearchitectureDescription[];
extern const char kContextualTasksEphemeralBrandedEntryPointName[];
extern const char kContextualTasksEphemeralBrandedEntryPointDescription[];
extern const char kContextualTasksUpdatedEntryPointsName[];
extern const char kContextualTasksUpdatedEntryPointsDescription[];
extern const char kContextualTasksSidePanelRearchitectureName[];
extern const char kContextualTasksSidePanelRearchitectureDescription[];
extern const char kContextualTasksClobberActiveTabName[];
extern const char kContextualTasksClobberActiveTabDescription[];
extern const char kContextualTasksTabBasedSuggestionsName[];
extern const char kContextualTasksTabBasedSuggestionsDescription[];
extern const char kContextualTasksSidePanelTabStateName[];
extern const char kContextualTasksSidePanelTabStateDescription[];
extern const char kContextualTasksBypassDismissedCapName[];
extern const char kContextualTasksBypassDismissedCapDescription[];
extern const char kEphemeralPinningVisibleWhenPermanentlyPinnedName[];
extern const char kEphemeralPinningVisibleWhenPermanentlyPinnedDescription[];
extern const char kContextualTasksEphemeralButtonContextMenuName[];
extern const char kContextualTasksEphemeralButtonContextMenuDescription[];
extern const char kContextualTasksEphemeralButtonAutoDismissName[];
extern const char kContextualTasksEphemeralButtonAutoDismissDescription[];
extern const char kContextualTasksUnifyPinButtonLogosName[];
extern const char kContextualTasksUnifyPinButtonLogosDescription[];
extern const char kABASearchJourneysName[];
extern const char kABASearchJourneysDescription[];
extern const char kAllowSignedOutUserInDesktopAndroidName[];
extern const char kAllowSignedOutUserInDesktopAndroidDescription[];

}  // namespace flag_descriptions

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
