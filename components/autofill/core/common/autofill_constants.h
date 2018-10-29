// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Autofill component.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_

#include <stddef.h>         // For size_t

namespace autofill {

// Help URL for the Autofill dialog.
extern const char kHelpURL[];

// The origin of an AutofillDataModel created or modified in the settings page.
extern const char kSettingsOrigin[];

// The number of fields required by Autofill to execute its heuristic and
// crowd-sourcing query/upload routines.
size_t MinRequiredFieldsForHeuristics();
size_t MinRequiredFieldsForQuery();
size_t MinRequiredFieldsForUpload();

// The minimum number of fields in a form that contains only password fields to
// upload the form to and request predictions from the Autofill servers.
const size_t kRequiredFieldsForFormsWithOnlyPasswordFields = 2;

// Special query id used between the browser and the renderer when the action
// is initiated from the browser.
const int kNoQueryId = -1;

// Options bitmask values for AutofillHostMsg_ShowPasswordSuggestions IPC
enum ShowPasswordSuggestionsOptions {
  SHOW_ALL = 1 << 0 /* show all credentials, not just ones matching username */,
  IS_PASSWORD_FIELD = 1 << 1 /* input field is a password field */
};

// Autofill StrikeDatabase: Maximum strikes allowed for the credit card save
// project. If the StrikeDatabase returns this many strikes for a given card, it
// will not show the offer-to-save bubble on Desktop or infobar on Android.
// On Desktop, however, the omnibox icon will still be available.
const int kMaxStrikesToPreventPoppingUpOfferToSavePrompt = 3;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
