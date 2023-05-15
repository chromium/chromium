// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Autofill component.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_

#include <stddef.h>  // For size_t

#include "base/time/time.h"

namespace autofill {

// The origin of an AutofillDataModel created or modified in the settings page.
extern const char kSettingsOrigin[];

// The number of fields required by Autofill to execute its heuristic and
// crowd-sourcing query/upload routines.
constexpr size_t kMinRequiredFieldsForHeuristics = 3;
constexpr size_t kMinRequiredFieldsForQuery = 1;
constexpr size_t kMinRequiredFieldsForUpload = 1;

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
constexpr size_t kAutofillManagerMaxFormCacheSize = 100;

// The maximum number of form fields we are willing to parse, due to
// computational costs. Several examples of forms with lots of fields that are
// not relevant to Autofill: (1) the Netflix queue; (2) the Amazon wishlist;
// (3) router configuration pages; and (4) other configuration pages, e.g. for
// Google code project settings.
// Copied to components/autofill/ios/form_util/resources/fill.js.
constexpr size_t kMaxParseableFields = 200;

// The maximum number of form fields we are willing to parse, due to
// computational costs.
constexpr size_t kMaxParseableChildFrames = 20;

// The maximum string length supported by Autofill. In particular, this is used
// for the length of field values.
// This limit prevents sending overly large strings via IPC to the browser
// process.
constexpr size_t kMaxStringLength = 1024;

// The maximum list size supported by Autofill.
// Allow enough space for all countries (roughly 300 distinct values) and all
// timezones (roughly 400 distinct values), plus some extra wiggle room.
// This limit prevents sending overly large strings via IPC to the browser
// process.
constexpr size_t kMaxListSize = 512;

// The maximum number of allowed calls to CreditCard::GetMatchingTypes() and
// AutofillProfile::GetMatchingTypeAndValidities().
// If #fields * (#profiles + #credit-cards) exceeds this number, type matching
// and voting is omitted.
// The rationale is that for a form with |kMaxParseableFields| = 200 fields,
// this still allows for 25 profiles plus credit cars.
constexpr size_t kMaxTypeMatchingCalls = 5000;

// The minimum number of fields in a form that contains only password fields to
// upload the form to and request predictions from the Autofill servers.
constexpr size_t kRequiredFieldsForFormsWithOnlyPasswordFields = 2;

// Options bitmask values for AutofillHostMsg_ShowPasswordSuggestions IPC
enum ShowPasswordSuggestionsOptions {
  SHOW_ALL = 1 << 0 /* show all credentials, not just ones matching username */,
  IS_PASSWORD_FIELD = 1 << 1 /* input field is a password field */,
  ACCEPTS_WEBAUTHN_CREDENTIALS =
      1 << 2 /* input field is marked to accept webauthn credentials */,
};

// A refill happens only within `kLimitBeforeRefill` of the original fill.
constexpr base::TimeDelta kLimitBeforeRefill = base::Seconds(1);

// Constants for the soft/hard deletion of Autofill data.
constexpr base::TimeDelta kDisusedDataModelTimeDelta = base::Days(180);
constexpr base::TimeDelta kDisusedDataModelDeletionTimeDelta = base::Days(395);

// Defines for how long recently submitted profile fragments are retained in
// memory for multi-step imports.
constexpr base::TimeDelta kMultiStepImportTTL = base::Minutes(5);

// Returns if the entry with the given |use_date| is deletable? (i.e. has not
// been used for a long time).
bool IsAutofillEntryWithUseDateDeletable(const base::Time& use_date);

// The period after which autocomplete entries should be cleaned-up in days.
// Equivalent to roughly 14 months.
constexpr base::TimeDelta kAutocompleteRetentionPolicyPeriod =
    base::Days(14 * 31);

// Limits the number of times the value of a specific type can be filled into a
// form.
// Credit card numbers are sometimes distributed between up to 19 individual
// fields. Therefore, credit cards need a higher limit.
// State fields are effecectively unlimited because there are sometimes hidden
// fields select boxes, each with a list of states for one specific countries,
// which are displayed only upon country selection.
constexpr size_t kTypeValueFormFillingLimit = 9;
constexpr size_t kCreditCardTypeValueFormFillingLimit = 19;
constexpr size_t kStateTypeValueFormFillingLimit = 1000;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
