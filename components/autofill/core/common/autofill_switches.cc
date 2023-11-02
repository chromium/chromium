// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_switches.h"

#include "build/build_config.h"

namespace autofill {
namespace switches {

// Sets the API key that will be used when calling Autofill API instead of
// using Chrome's baked key by default. You can use this to test new versions
// of the API that are not linked to the Chrome baked key yet.
const char kAutofillAPIKey[] = "autofill-api-key";

// Override the default autofill server URL with "scheme://host[:port]/prefix/".
const char kAutofillServerURL[] = "autofill-server-url";

// The number of days after which to reset the registry of autofill events for
// which an upload has been sent.
const char kAutofillUploadThrottlingPeriodInDays[] =
    "autofill-upload-throttling-period-in-days";

// Ignores autocomplete="off" for Autofill data (profiles + credit cards).
const char kIgnoreAutocompleteOffForAutofill[] =
    "ignore-autocomplete-off-autofill";

// Annotates forms with Autofill field type predictions.
const char kShowAutofillTypePredictions[] = "show-autofill-type-predictions";

// Annotates forms and fields with Autofill signatures.
const char kShowAutofillSignatures[] = "show-autofill-signatures";

// Use the sandbox Online Wallet service URL (for developer testing).
const char kWalletServiceUseSandbox[] = "wallet-service-use-sandbox";

}  // namespace switches
}  // namespace autofill
