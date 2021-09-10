// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_

namespace autofill {

// The list of all Autofill popup types that can be presented to the user.
enum class PopupType {
  kUnspecified,
  // Address form, but no address-related field is present. For example, it's
  // a sign-up page in which the user only enters the name and the email.
  kPersonalInformation,
  // Address form with address-related fields.
  kAddresses,
  kCreditCards,
  kPasswords,
};

// This reason is passed whenever a popup needs to be closed.
enum class PopupHidingReason {
  kAcceptSuggestion,        // A suggestion was accepted.
  kAttachInterstitialPage,  // An interstitial page displaces the popup.
  kEndEditing,    // A field isn't edited anymore but remains focused for now.
  kFocusChanged,  // Focus removed from field. Follows kEndEditing.
  kContentAreaMoved,  // Scrolling or zooming into the page displaces popup.
  kNavigation,        // A navigation on page or frame level.
  kNoSuggestions,     // The popup is or would become empty.
  kRendererEvent,     // The renderer explicitly requested closing the popup.
  kTabGone,  // The tab with popup is destroyed, hidden or has become inactive.
  kStaleData,      // The popup contains stale data.
  kUserAborted,    // The user explicitly dismissed the popup (e.g. ESC key).
  kViewDestroyed,  // The popup view (or its controller) goes out of scope.
  kWidgetChanged,  // The platform-native UI changed (e.g. window resize).
  kInsufficientSpace,  // Not enough space in content area to display an display
                       // at least one row of the popup within the bounds of the
                       // content area.
  kOverlappingWithAnotherPrompt,  // If the popup will be drawn, it will overlap
                                  // with another open prompt, and may hide
                                  // sensitive information in the prompt.
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_
