// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/pref_names.h"

namespace unified_consent {
namespace prefs {

// Boolean indicating whether all unified consent services were ever enabled
// because the user opted into unified consent. This pref is used during
// rollback to disable off-by-default services.
const char kAllUnifiedConsentServicesWereEnabled[] =
    "unified_consent.all_services_were_enabled";

// Boolean indicating whether the user had everything synced before migrating to
// unified consent.
const char kHadEverythingSyncedBeforeMigration[] =
    "unified_consent.had_everything_synced_before_migration";

// Boolean indicating whether all criteria is met for the consent bump to be
// shown.
const char kShouldShowUnifiedConsentBump[] =
    "unified_consent.consent_bump.should_show";

// Boolean that is true when the user opted into unified consent.
const char kUnifiedConsentGiven[] = "unified_consent_given";

// Integer indicating the migration state of unified consent, defined in
// unified_consent::MigrationState.
const char kUnifiedConsentMigrationState[] = "unified_consent.migration_state";

const char kUrlKeyedAnonymizedDataCollectionEnabled[] =
    "url_keyed_anonymized_data_collection.enabled";

}  // namespace prefs
}  // namespace unified_consent
