// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_INTERACTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_INTERACTION_H_

namespace autofill {

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused.
// Used to log user behaviour metrics for both the root and subpopups on
// desktop.
enum class PopupInteraction {
  kPopupShown,
  kSuggestionSelected,
  kSuggestionAccepted,
  kMaxValue = kSuggestionAccepted,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_INTERACTION_H_
