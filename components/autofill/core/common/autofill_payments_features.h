// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
struct Feature;
}

namespace autofill {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillAlwaysReturnCloudTokenizedCard;
extern const base::Feature kAutofillCreditCardAblationExperiment;
extern const base::Feature kAutofillCreditCardAuthentication;
extern const base::Feature kAutofillCreditCardUploadFeedback;
extern const base::Feature kAutofillDoNotMigrateUnsupportedLocalCards;
extern const base::Feature kAutofillEnableLocalCardMigrationForNonSyncUser;
extern const base::Feature kAutofillEnableToolbarStatusChip;
extern const base::Feature kAutofillNoLocalSaveOnUnmaskSuccess;
extern const base::Feature kAutofillNoLocalSaveOnUploadSuccess;
extern const base::Feature kAutofillSaveCardDismissOnNavigation;
extern const base::Feature kAutofillUpdatedCardUnmaskPromptUi;
extern const base::Feature kAutofillUpstream;
extern const base::Feature kAutofillUpstreamAllowAllEmailDomains;
extern const base::Feature kAutofillUpstreamAlwaysRequestCardholderName;
extern const base::Feature kAutofillUpstreamBlankCardholderNameField;
extern const base::Feature kAutofillUpstreamEditableCardholderName;
extern const base::Feature kAutofillUpstreamEditableExpirationDate;

// Return whether a [No thanks] button and new messaging is shown in the save
// card bubbles. This will be called only on desktop platforms.
bool ShouldShowImprovedUserConsentForCreditCardSave();

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
