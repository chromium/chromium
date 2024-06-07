// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_HIDING_REASON_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_HIDING_REASON_H_

namespace autofill {

// This reason is passed whenever a popup needs to be closed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// When adding a value to this enum, please update
// tools/metrics/histograms/metadata/autofill/enums.xml.
enum class SuggestionHidingReason {
  // A suggestion was accepted.
  kAcceptSuggestion = 0,
  // An interstitial page displaces the popup.
  kAttachInterstitialPage = 1,
  // The text field is no longer edited - sent directly before a focus change.
  // TODO(crbug.com/40277556): Deprecate in favor of kFocusChanged.
  kEndEditing = 2,
  // Focus removed from field. Follows kEndEditing.
  kFocusChanged = 3,
  // Scrolling or zooming into the page displaces the popup.
  kContentAreaMoved = 4,
  // A navigation on the page or frame level.
  kNavigation = 5,
  // The popup is or would become empty.
  kNoSuggestions = 6,
  // The renderer explicitly requested closing the popup.
  kRendererEvent = 7,
  // The tab with the popup is destroyed, hidden or has become inactive.
  kTabGone = 8,
  // The popup contains stale data.
  kStaleData = 9,
  // The user explicitly dismissed the popup (e.g. ESC key).
  kUserAborted = 10,
  // The popup view (or its controller) goes out of scope.
  kViewDestroyed = 11,
  // The platform-native UI changed (e.g. window resize).
  kWidgetChanged = 12,
  // Not enough space in content area to display an display at least one row of
  // the popup within the bounds of the content area.
  kInsufficientSpace = 13,
  // The popup would overlap with another open prompt, and may hide sensitive
  // information in the prompt.
  kOverlappingWithAnotherPrompt = 14,
  // The anchor element for which the popup would be shown is not visible in the
  // content area.
  kElementOutsideOfContentArea = 15,
  // The frame holds a pointer lock.
  kMouseLocked = 16,
  // The password generation popup would overlap and hide autofill popup.
  kOverlappingWithPasswordGenerationPopup = 17,
  // The Touch To Fill surface is shown instead of autofill suggestions.
  kOverlappingWithTouchToFillSurface = 18,
  // The context menu is shown instead of the autofill suggestions.
  kOverlappingWithAutofillContextMenu = 19,
  // The Fast Checkout surface is shown instead of autofill suggestions.
  kOverlappingWithFastCheckoutSurface = 20,
  // The picture-in-picture window overlaps with the autofill suggestions.
  kOverlappingWithPictureInPictureWindow = 21,
  // The context menu was opened. We hide the autofill popup to make sure it
  // does not overlap with it.
  kContextMenuOpened = 22,
  // No frame currently has focus. This case is caught for safety because it
  // might be reachable due to race conditions.
  kNoFrameHasFocus = 23,
  // Sub-popup related reason, used when closing a sub-popup (e.g. by moving
  // the mouse out of the suggestion control or by the keyboard navigation).
  kExpandedSuggestionCollapsedSubPopup = 24,
  // The field's value changed. Currently, this event is only used for
  // Compose-related
  // popups, which close in response to user edits of the focused field.
  kFieldValueChanged = 25,
  // When a Compose popup loses focus, another autofill popup is shown for
  // a few seconds on a timer then the popup is closed. Currently, this event
  // is only used for Compose.
  kFadeTimerExpired = 26,
  // The popup search bar loses focus.
  kSearchBarFocusLost = 27,
  kMaxValue = kSearchBarFocusLost
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_HIDING_REASON_H_
