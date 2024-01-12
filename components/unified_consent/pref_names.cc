// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/pref_names.h"

namespace unified_consent {
namespace prefs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kUnifiedConsentMigrationState[] = "unified_consent.migration_state";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
const char kUrlKeyedAnonymizedDataCollectionEnabled[] =
    "url_keyed_anonymized_data_collection.enabled";

const char kPageContentCollectionEnabled[] = "page_content_collection.enabled";

}  // namespace prefs
}  // namespace unified_consent
