// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_PREF_NAMES_H_
#define COMPONENTS_UNIFIED_CONSENT_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace unified_consent::prefs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Integer indicating the migration state of unified consent, defined in
// unified_consent::MigrationState.
//
// Note: Migration of profiles to unified consent is over on all platforms
// except on ChromeOS where the user profiles start with sync enabled.
//
// TODO(http://crbug.com/1459986): Remove this from ChromeOS Ash as well.
extern const char kUnifiedConsentMigrationState[];
#endif

// Boolean indicating whether anonymized URL-keyed data data collection (aka
// "Make Searches and Browsing Better") is enabled.
// NOTE: Instead of directly reading this pref, use
// UrlKeyedDataCollectionConsentHelper::
//     NewAnonymizedDataCollectionConsentHelper.
extern const char kUrlKeyedAnonymizedDataCollectionEnabled[];

// Whether the user has enabled sharing page content.
extern const char kPageContentCollectionEnabled[];

}  // namespace unified_consent::prefs

#endif  // COMPONENTS_UNIFIED_CONSENT_PREF_NAMES_H_
