// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_

namespace base {
class TimeDelta;
}  // namespace base

namespace compose {

// Compose histogram names.
extern const char kComposeResponseDurationOk[];
extern const char kComposeResponseDurationError[];
extern const char kComposeResponseStatus[];
extern const char kComposeShowStatus[];
extern const char kComposeDialogOpenLatency[];

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
// Keep in sync with ComposeContextMenuCtrEvent in
// src/tools/metrics/histograms/enums.xml.
enum class ComposeSessionCloseReason {
  kAcceptedSuggestion = 0,
  kCloseButtonPressed = 1,
  kEndedImplicitly = 2,
  kMaxValue = kEndedImplicitly,
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
  kDisabledMsbb = 3,
  kSignedOut = 4,
  kUnsupportedLanguage = 5,
  kFormFieldInCrossOriginFrame = 6,
  kMaxValue = kFormFieldInCrossOriginFrame,
};

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event);

void LogComposeContextMenuShowStatus(ComposeShowStatus status);

// Log the duration of a compose request. |is_valid| indicates the status of
// the request.
void LogComposeRequestDuration(base::TimeDelta duration, bool is_ok);

void LogComposeSessionCloseReason(ComposeSessionCloseReason reason);

// Log the time taken for the dialog to be fully shown and interactable.
void LogComposeDialogOpenLatency(base::TimeDelta duration);
}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
