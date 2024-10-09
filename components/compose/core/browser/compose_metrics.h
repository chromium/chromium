// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_

#include "base/time/time.h"
#include "components/compose/core/browser/compose_enums.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace compose {

// Compose histogram names.
extern const char kComposeDialogOpenLatency[];
extern const char kComposeDialogSelectionLength[];
extern const char kComposeRequestReason[];
extern const char kComposeRequestDurationOkSuffix[];
extern const char kComposeRequestDurationErrorSuffix[];
extern const char kComposeRequestStatus[];
extern const char kComposeSessionComposeCount[];
extern const char kComposeSessionCloseReason[];
extern const char kComposeSessionDialogShownCount[];
extern const char kComposeSessionEventCounts[];
extern const char kComposeSessionDuration[];
extern const char kComposeSessionOverOneDay[];
extern const char kComposeSessionUndoCount[];
extern const char kComposeSessionUpdateInputCount[];
extern const char kComposeShowStatus[];
extern const char kComposeFirstRunSessionCloseReason[];
extern const char kComposeFirstRunSessionDialogShownCount[];
extern const char kComposeMSBBSessionCloseReason[];
extern const char kComposeMSBBSessionDialogShownCount[];
extern const char kInnerTextNodeOffsetFound[];
extern const char kComposeContextMenuCtr[];
extern const char kComposeProactiveNudgeCtr[];
extern const char kComposeSelectionNudgeCtr[];
extern const char kComposeProactiveNudgeShowStatus[];
extern const char kOpenComposeDialogResult[];
extern const char kComposeStartSessionEntryPoint[];
extern const char kComposeResumeSessionEntryPoint[];

// Enum for calculating the CTR of the Compose context menu item.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// ComposeContextMenuCtrEvent in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeContextMenuCtrEvent {
  kMenuItemDisplayed = 0,
  kMenuItemClicked = 1,
  kMaxValue = kMenuItemClicked,
};

// Keep in sync with ComposeRequestReason in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeRequestReason {
  // When the ComposeUpfrontInputModes featuer is enabled, the "first request"
  // is split between one of three input modes.
  // TODO(b/371054228): Deprecate the kFirstRequest bucket when upfront inputs
  // launches.
  kFirstRequest = 0,
  kRetryRequest = 1,
  kUpdateRequest = 2,
  kLengthShortenRequest = 3,
  kLengthElaborateRequest = 4,
  kToneCasualRequest = 5,
  kToneFormalRequest = 6,
  kFirstRequestPolishMode = 7,
  kFirstRequestElaborateMode = 8,
  kFirstRequestFormalizeMode = 9,
  kMaxValue = kFirstRequestFormalizeMode,
};

// Close reasons for sessions that start with FRE or MSBB dialogs.
// Keep in sync with ComposeFreOrMsbbSessionCloseReasonType in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeFreOrMsbbSessionCloseReason {
  kAbandoned = 0,
  kCloseButtonPressed = 1,
  kAckedOrAcceptedWithoutInsert = 2,
  kAckedOrAcceptedWithInsert = 3,
  kReplacedWithNewSession = 4,
  kExceededMaxDuration = 5,
  kMaxValue = kExceededMaxDuration,
};

// Keep in sync with ComposeSessionCloseReasonType in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeSessionCloseReason {
  kInsertedResponse = 0,
  kCloseButtonPressed = 1,
  kAbandoned = 2,  // Tab closed or navigated away with an open session.
  kReplacedWithNewSession = 3,
  kCanceledBeforeResponseReceived =
      4,  // Close button pressed with pending navigation.
  kExceededMaxDuration = 5,
  kEndedAtFre = 6,
  kAckedFreEndedAtMsbb = 7,
  kEndedAtMsbb = 8,
  kMaxValue = kEndedAtMsbb,
};

// Keep in sync with ComposeSessionEventCounts in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeSessionEventTypes {
  kMainDialogShown = 0,
  kFREShown = 1,
  kFREAccepted = 2,
  kMSBBShown = 3,
  kMSBBSettingsOpened = 4,
  kMSBBEnabled = 5,
  kStartedWithSelection = 6,
  kCreateClicked = 7,
  kUpdateClicked = 8,
  kRetryClicked = 9,
  kUndoClicked = 10,
  kShortenClicked = 11,
  kElaborateClicked = 12,
  kCasualClicked = 13,
  kFormalClicked = 14,
  kThumbsDown = 15,
  kThumbsUp = 16,
  kInsertClicked = 17,
  kCloseClicked = 18,
  kEditClicked = 19,
  kCancelEditClicked = 20,
  kAnyModifierUsed = 21,
  kRedoClicked = 22,
  kResultEdited = 23,
  kEditedResultInserted = 24,
  kSuccessfulRequest = 25,
  kFailedRequest = 26,
  kComposeDialogOpened = 27,
  kMaxValue = kComposeDialogOpened,
};

// Enum for recording the show status of both the HMW context menu item and
// the proactive nudge. These values are persisted to logs. Entries should not
// be renumbered and numeric values should never be reused. Keep in sync with
// ComposeShowStatus in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeShowStatus {
  kShouldShow = 0,
  kGenericBlocked = 1,
  kIncompatibleFieldType = 2,
  // DEPRECATED: there is a MSBB dialog now.
  // kDisabledMsbb = 3,
  kSignedOut = 4,
  kUnsupportedLanguage = 5,
  kFormFieldInCrossOriginFrame = 6,
  kPerUrlChecksFailed = 7,
  kUserNotAllowedByOptimizationGuide = 8,
  kNotComposeEligible = 9,
  kIncorrectScheme = 10,
  kFormFieldNestedInFencedFrame = 11,
  kComposeFeatureFlagDisabled = 12,
  kDisabledOnChromeOS = 13,
  kAutocompleteOff = 14,
  kWritingSuggestionsFalse = 15,
  kProactiveNudgeFeatureDisabled = 16,
  kProactiveNudgeDisabledGloballyByUserPreference = 17,
  kProactiveNudgeDisabledForSiteByUserPreference = 18,
  kProactiveNudgeDisabledByServerConfig = 19,
  kProactiveNudgeUnknownServerConfig = 20,
  // DEPRECATED: now using the segmentation platform.
  // kRandomlyBlocked = 21,
  kProactiveNudgeDisabledByMSBB = 22,
  kProactiveNudgeBlockedBySegmentationPlatform = 23,
  kComposeNotEnabledInCountry = 24,
  kUndefinedCountry = 25,
  kMaxValue = kUndefinedCountry,
};

// Enum for calculating the CTR of the Compose proactive nudge.
// Keep in sync with ComposeNudgeCtrEvent in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeNudgeCtrEvent {
  kNudgeDisplayed = 0,
  kDialogOpened = 1,
  kUserDisabledProactiveNudge = 2,
  kUserDisabledSite = 3,
  kOpenSettings = 4,
  kMaxValue = kOpenSettings,
};

// Enum for recording the entry point for starting or resuming a Compose
// session. Keep in sync with ComposeEntryPoint in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeEntryPoint {
  kContextMenu = 0,
  kProactiveNudge = 1,
  kSelectionNudge = 2,
  kSavedStateNudge = 3,
  kSavedStateNotification = 4,
  kMaxValue = kSavedStateNotification,
};

enum class EvalLocation : int {
  // Response was evaluated on the server.
  kServer,
  // Response was evaluated on the device.
  kOnDevice,
};

// Keep in sync with ComposeSessionEvalLocation in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class SessionEvalLocation {
  // No responses were evaluated.
  kNone = 0,
  // All responses were evaluated on the server.
  kServer = 1,
  // All responses were evaluated on the device.
  kOnDevice = 2,
  // Some responses were evaluated on the server and some on the device.
  kMixed = 3,
  kMaxValue = kMixed,
};

// Enum for recording the feedback state of a Compose request.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// ComposeRequestFeedback in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeRequestFeedback {
  kNoFeedback = 0,
  kPositiveFeedback = 1,
  kNegativeFeedback = 2,
  kRequestError = 3,
  kMaxValue = kRequestError,
};

// The output metric for the proactive nudge segmentation model. Represents what
// effect the nudge had on the user's engagement. Keep in sync with
// ProactiveNudgeDerivedEngagement in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ProactiveNudgeDerivedEngagement {
  // The user didn't interact with the nudge.
  kIgnored,
  // The user disabled the nudge on this site using the three-dot menu.
  kNudgeDisabledOnSingleSite,
  // The user disabled the nudge on all sites using the three-dot menu.
  kNudgeDisabledOnAllSites,
  // User clicked the nudge, but didn't press generate in Compose.
  kOpenedComposeMinimalUse,
  // User clicked the nudge, pressed generate at least once, but didn't accept
  // the suggestion.
  kGeneratedComposeSuggestion,
  // User clicked, pressed generate, and accepted a suggestion.
  kAcceptedComposeSuggestion,
  kMaxValue = kAcceptedComposeSuggestion,
};

// Struct containing event and logging information for an individual
// |ComposeSession|.
struct ComposeSessionEvents {
  ComposeSessionEvents();
  ComposeSessionEvents(ComposeSessionEvents& e) = delete;
  ComposeSessionEvents& operator=(ComposeSessionEvents& e) = delete;
  ~ComposeSessionEvents() = default;

  // Logging counters.
  // Times we have opened Compose to any section (main, FRE, or MSBB).
  unsigned int compose_dialog_open_count = 0;
  // Times we have shown the Compose prompot (i.e. past the FRE & MSBB).
  unsigned int compose_prompt_view_count = 0;
  // The total number of Compose Requests for the session.
  unsigned int compose_requests_count = 0;
  // The total number of successful Compose requests.
  unsigned int successful_requests_count = 0;
  // The total number of Compose requests with an error.
  unsigned int failed_requests_count = 0;
  // Times we have shown the first run view.
  unsigned int fre_view_count = 0;
  // Times we have shown the view to enable MSBB.
  unsigned int msbb_view_count = 0;
  // Times the user has pressed "undo" this session.
  unsigned int undo_count = 0;
  // Times the user has pressed "redo" this session.
  unsigned int redo_count = 0;
  // Times the user has edited the result text this session.
  unsigned int result_edit_count = 0;
  // Compose request after input edited.
  unsigned int update_input_count = 0;
  // Times the user has pressed the "Retry" button.
  unsigned int regenerate_count = 0;
  // Times the user has picked the "Shorter" option.
  unsigned int shorten_count = 0;
  // Times the user has picked the "Elaborate" option.
  unsigned int lengthen_count = 0;
  // Times the user has picked the "Formal" option.
  unsigned int formal_count = 0;
  // Times the user has picked the "Casual" option.
  unsigned int casual_count = 0;

  // Logging flags
  // True if the FRE was completed in the session.
  bool fre_completed_in_session = false;
  // True if the MSBB settings were opened.
  bool msbb_settings_opened = false;
  // True if the MSBB was enabled in the session.
  bool msbb_enabled_in_session = false;

  // True if the session started from the proactive nudge.
  bool started_with_proactive_nudge = false;
  // True if the session started with selected text.
  bool has_initial_text = false;
  // True if thumbs up was ever clicked during the session.
  bool has_thumbs_up = false;
  // True if thumbs down was ever clicked during the session.
  bool has_thumbs_down = false;

  // True if the results were eventually inserted back to the web page.
  bool inserted_results = false;
  // True if an edited result was eventually inserted back to the web page.
  bool edited_result_inserted = false;
  // True if the the user closed the compose session via the "x" button.
  bool close_clicked = false;
  // True if the user has pressed the "Edit" button this session.
  bool did_click_edit = false;
  // True if the user has pressed "Cancel" on the editing view for this session.
  bool did_click_cancel_on_edit = false;
  // Number of on-device responses received.
  unsigned int on_device_responses = 0;
  // Number of server responses received.
  unsigned int server_responses = 0;

  // True if amy compose response was filtered
  bool session_contained_filtered_response = false;
  // True if any compose response contained error
  bool session_contained_any_error = false;
};

// Enum with the possible reasons for it being impossible to open the Compose
// dialog after the user requested it.
// Keep in sync with OpenComposeDialogResult in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class OpenComposeDialogResult {
  kSuccess = 0,
  kNoChromeComposeClient = 1,
  kNoRenderFrameHost = 2,
  kNoContentAutofillDriver = 3,
  kAutofillFormDataNotFound = 4,
  kAutofillFormFieldDataNotFound = 5,
  kNoWebContents = 6,
  kFailedCreatingComposeDialogView = 7,
  kAutofillFormDataNotFoundAfterSelectAll = 8,
  kMaxValue = kAutofillFormDataNotFoundAfterSelectAll
};

// Enum to log if the inner text successfuly found an offset
// Keep in sync with ComposeInnerTextNodeOffset in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeInnerTextNodeOffset {
  kNoOffsetFound = 0,
  kOffsetFound = 1,
  kMaxValue = kOffsetFound
};

// Enum to log if all text was selected on behalf of the user.
// Keep in sync with ComposeSelectAllStatus in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeSelectAllStatus {
  kNoSelectAll = 0,
  kSelectedAll = 1,
  kMaxValue = kSelectedAll
};

// Class that automatically reports any UKM metrics for the page-level Compose
// UKM as defined in go/ukm-collection-chrome-compose.
class PageUkmTracker {
 public:
  PageUkmTracker(ukm::SourceId source_id);
  ~PageUkmTracker();

  // The compose menu item was shown in a context menu.
  void MenuItemShown();

  // The compose menu item was clicked, opening Compose.
  void MenuItemClicked();

  // The composed text was accepted and inserted into the webpage by the user.
  void ComposeTextInserted();

  // Records that the proactive nudge should show. Recorded anytime the
  // proactive nudge could be shown even if the nudge is eventually blocked.
  void ComposeProactiveNudgeShouldShow();

  // The compose proactive nudge was shown.
  void ProactiveNudgeShown();

  // The compose proactive nudge was opened.
  void ProactiveNudgeOpened();

  // Mark that proactive nudge preferences were set during this page load.
  void ProactiveNudgeDisabledGlobally();
  void ProactiveNudgeDisabledForSite();

  // The compose dialog was requested but not shown due to problems obtaining
  // form data from Autofill.
  void ShowDialogAbortedDueToMissingFormData();
  void ShowDialogAbortedDueToMissingFormFieldData();

  // Records UKM if any of the above events happened during this object's
  // lifetime.  Called in the destructor.
  void MaybeLogUkm();

 private:
  bool event_was_recorded_ = false;
  unsigned int menu_item_shown_count_ = 0;
  unsigned int menu_item_clicked_count_ = 0;

  unsigned int compose_text_inserted_count_ = 0;

  unsigned int proactive_nudge_should_show_count_ = 0;
  unsigned int proactive_nudge_shown_count_ = 0;
  unsigned int proactive_nudge_opened_count_ = 0;

  bool proactive_nudge_disabled_globally_ = false;
  bool proactive_nudge_disabled_for_site_ = false;

  unsigned int missing_form_data_count_ = 0;
  unsigned int missing_form_field_data_count_ = 0;

  ukm::SourceId source_id;
};

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event);

void LogComposeContextMenuShowStatus(ComposeShowStatus status);

void LogComposeProactiveNudgeCtr(ComposeNudgeCtrEvent event);

void LogComposeSelectionNudgeCtr(ComposeNudgeCtrEvent event);

void LogComposeProactiveNudgeShowStatus(ComposeShowStatus status);

void LogOpenComposeDialogResult(OpenComposeDialogResult result);

void LogStartSessionEntryPoint(ComposeEntryPoint entry_point);
void LogResumeSessionEntryPoint(ComposeEntryPoint entry_point);

void LogComposeRequestReason(ComposeRequestReason reason);
void LogComposeRequestReason(EvalLocation eval_location,
                             ComposeRequestReason reason);

void LogComposeRequestStatus(bool page_language_supported,
                             compose::mojom::ComposeStatus status);
void LogComposeRequestStatus(EvalLocation eval_location,
                             bool page_language_supported,
                             compose::mojom::ComposeStatus status);

// Log the duration of a compose request. |is_ok| indicates the status of
// the request.
void LogComposeRequestDuration(base::TimeDelta duration,
                               EvalLocation eval_location,
                               bool is_ok);

void LogComposeSessionCloseReason(ComposeSessionCloseReason reason);

void LogComposeFirstRunSessionCloseReason(
    ComposeFreOrMsbbSessionCloseReason reason);

// Log session based metrics when a FRE session ends.
void LogComposeFirstRunSessionDialogShownCount(
    ComposeFreOrMsbbSessionCloseReason reason,
    int dialog_shown_count);

void LogComposeMSBBSessionCloseReason(
    ComposeFreOrMsbbSessionCloseReason reason);

// Log session based metrics when a consent session ends.
void LogComposeMSBBSessionDialogShownCount(
    ComposeFreOrMsbbSessionCloseReason reason,
    int dialog_shown_count);

SessionEvalLocation GetSessionEvalLocationFromEvents(
    const ComposeSessionEvents& session_events);

std::optional<EvalLocation> GetEvalLocationFromEvents(
    const ComposeSessionEvents& session_events);

// Log session based metrics when a session ends.
// Should only be called once per session.
void LogComposeSessionCloseMetrics(ComposeSessionCloseReason reason,
                                   const ComposeSessionEvents& session_events);

// Log session based UKM metrics when the session ends.
void LogComposeSessionCloseUkmMetrics(
    ukm::SourceId source_id,
    const ComposeSessionEvents& session_events);

// Log the amount trimmed from the inner text from the page (in bytes) when the
// dialog is opened.
void LogComposeDialogInnerTextShortenedBy(int shortened_by);

// Log the size (in bytes) of the untrimmed inner text from the page when the
// dialog is opened.
void LogComposeDialogInnerTextSize(int size);

// Log if the inner text node offset was found successfully.
void LogComposeDialogInnerTextOffsetFound(bool inner_offset_found);

// Log the time taken for the dialog to be fully shown and interactable.
void LogComposeDialogOpenLatency(base::TimeDelta duration);

// Log the character length of the selection when the dialog is opened.
void LogComposeDialogSelectionLength(int length);

// Log the session duration with |session_suffix| applied to histogram name.
void LogComposeSessionDuration(
    base::TimeDelta session_duration,
    std::string session_suffix = "",
    std::optional<EvalLocation> eval_location = std::nullopt);

void LogComposeRequestFeedback(EvalLocation eval_location,
                               ComposeRequestFeedback feedback);

void LogComposeSelectAllStatus(ComposeSelectAllStatus select_all_status);

// Emit an enum for for each event present in `session_events`.
// Split the event counts histogram on `eval_location` if provided.
void LogComposeSessionEventCounts(std::optional<EvalLocation> eval_location,
                                  const ComposeSessionEvents& session_events);

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
