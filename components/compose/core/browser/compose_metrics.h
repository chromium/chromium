// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace compose {

// Compose histogram names.
extern const char kComposeDialogOpenLatency[];
extern const char kComposeDialogSelectionLength[];
extern const char kComposeRequestReason[];
extern const char kComposeRequestDurationOk[];
extern const char kComposeRequestDurationError[];
extern const char kComposeRequestStatus[];
extern const char kComposeSessionComposeCount[];
extern const char kComposeSessionCloseReason[];
extern const char kComposeSessionDialogShownCount[];
extern const char kComposeSessionUndoCount[];
extern const char kComposeSessionUpdateInputCount[];
extern const char kComposeShowStatus[];
extern const char kComposeFirstRunSessionCloseReason[];
extern const char kComposeFirstRunSessionDialogShownCount[];
extern const char kComposeMSBBSessionCloseReason[];
extern const char kComposeMSBBSessionDialogShownCount[];
extern const char kOpenComposeDialogResult[];

// Enum for calculating the CTR of the Compose context menu item.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// ComposeContextMenuCtrEvent in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeContextMenuCtrEvent {
  kMenuItemDisplayed = 0,
  kComposeOpened = 1,
  kMaxValue = kComposeOpened,
};

// Keep in sync with ComposeRequestReason in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeRequestReason {
  kFirstRequest = 0,
  kRetryRequest = 1,
  kUpdateRequest = 2,
  kLengthShortenRequest = 3,
  kLengthElaborateRequest = 4,
  kToneCasualRequest = 5,
  kToneFormalRequest = 6,
  kMaxValue = kToneFormalRequest,
};

// Keep in sync with ComposeMSBBSessionCloseReasonType in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeMSBBSessionCloseReason {
  kMSBBEndedImplicitly = 0,
  kMSBBCloseButtonPressed = 1,
  kMSBBAcceptedWithoutInsert = 2,
  kMSBBAcceptedWithInsert = 3,
  kMaxValue = kMSBBAcceptedWithInsert,
};

// Keep in sync with ComposeFirstRunSessionCloseReasonType in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeFirstRunSessionCloseReason {
  kEndedImplicitly = 0,
  kCloseButtonPressed = 1,
  kFirstRunDisclaimerAcknowledgedWithoutInsert = 2,
  kFirstRunDisclaimerAcknowledgedWithInsert = 3,
  kNewSessionWithSelectedText = 4,
  kMaxValue = kNewSessionWithSelectedText,
};

// Keep in sync with ComposeSessionCloseReasonType in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeSessionCloseReason {
  kAcceptedSuggestion = 0,
  kCloseButtonPressed = 1,
  kEndedImplicitly = 2,
  kNewSessionWithSelectedText = 3,
  kMaxValue = kNewSessionWithSelectedText,
};

// Enum for recording the show status of the Compose context menu item.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// ComposeShowStatus in
// src/tools/metrics/histograms/metadata/compose/enums.xml.
enum class ComposeShowStatus {
  kShouldShow = 0,
  kGenericBlocked = 1,
  kIncompatibleFieldType = 2,
  // kDisabledMsbb is no longer used now that we have a MSBB dialog.
  kDisabledMsbb = 3,  // obsolete
  kSignedOut = 4,
  kUnsupportedLanguage = 5,
  kFormFieldInCrossOriginFrame = 6,
  kPerUrlChecksFailed = 7,
  kUserNotAllowedByOptimizationGuide = 8,
  kNotComposeEligible = 9,
  kIncorrectScheme = 10,
  kFormFieldNestedInFencedFrame = 11,
  kFeatureFlagDisabled = 12,
  kMaxValue = kFeatureFlagDisabled,
};

// Struct containing event and logging information for an individual
// |ComposeSession|.
struct ComposeSessionEvents {
  // Logging counters.
  unsigned int compose_count = 0;
  unsigned int dialog_shown_count = 0;
  unsigned int fre_dialog_shown_count = 0;
  unsigned int msbb_dialog_shown_count = 0;
  unsigned int undo_count = 0;
  unsigned int update_input_count = 0;
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
  kMaxValue = kFailedCreatingComposeDialogView
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

  // Records UKM if any of the above events happened during this object's
  // lifetime.  Called in the destructor.
  void MaybeLogUkm();

 private:
  bool event_was_recorded_ = false;
  unsigned int menu_item_shown_count_ = 0;
  unsigned int menu_item_clicked_count_ = 0;
  unsigned int compose_text_inserted_count_ = 0;

  ukm::SourceId source_id;
};

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event);

void LogComposeContextMenuShowStatus(ComposeShowStatus status);

void LogOpenComposeDialogResult(OpenComposeDialogResult result);

void LogComposeRequestReason(ComposeRequestReason reason);

// Log the duration of a compose request. |is_valid| indicates the status of
// the request.
void LogComposeRequestDuration(base::TimeDelta duration, bool is_ok);

void LogComposeFirstRunSessionCloseReason(
    ComposeFirstRunSessionCloseReason reason);

// Log session based metrics when a FRE session ends.
void LogComposeFirstRunSessionDialogShownCount(
    ComposeFirstRunSessionCloseReason reason,
    int dialog_shown_count);

void LogComposeMSBBSessionCloseReason(ComposeMSBBSessionCloseReason reason);

// Log session based metrics when a consent session ends.
void LogComposeMSBBSessionDialogShownCount(ComposeMSBBSessionCloseReason reason,
                                           int dialog_shown_count);

// Log session based metrics when a session ends.
void LogComposeSessionCloseMetrics(ComposeSessionCloseReason reason,
                                   ComposeSessionEvents session_events);

// Log the amount trimmed from the inner text from the page (in bytes) when the
// dialog is opened.
void LogComposeDialogInnerTextShortenedBy(int shortened_by);

// Log the size (in bytes) of the untrimmed inner text from the page when the
// dialog is opened.
void LogComposeDialogInnerTextSize(int size);

// Log the time taken for the dialog to be fully shown and interactable.
void LogComposeDialogOpenLatency(base::TimeDelta duration);

// Log the character length of the selection when the dialog is opened.
void LogComposeDialogSelectionLength(int length);

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
