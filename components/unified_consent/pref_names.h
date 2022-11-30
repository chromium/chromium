// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_PREF_NAMES_H_
#define COMPONENTS_UNIFIED_CONSENT_PREF_NAMES_H_

namespace unified_consent {
namespace prefs {

// Integer indicating the migration state of unified consent, defined in
// unified_consent::MigrationState.
extern const char kUnifiedConsentMigrationState[];

// Boolean indicating whether anonymized URL-keyed data data collection is
// enabled.
extern const char kUrlKeyedAnonymizedDataCollectionEnabled[];

}  // namespace prefs
}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_PREF_NAMES_H_
