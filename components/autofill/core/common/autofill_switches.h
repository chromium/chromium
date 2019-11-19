// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_

#include "build/build_config.h"

namespace autofill {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAutofillAPIKey[];
extern const char kAutofillServerURL[];
extern const char kAutofillMetadataUploadEncoding[];
extern const char kAutofillUploadThrottlingPeriodInDays[];
extern const char kDisableOfferStoreUnmaskedWalletCards[];
extern const char kEnableOfferStoreUnmaskedWalletCards[];
extern const char kIgnoreAutocompleteOffForAutofill[];
extern const char kShowAutofillTypePredictions[];
extern const char kShowAutofillSignatures[];
extern const char kWalletServiceUseSandbox[];

}  // namespace switches
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_
