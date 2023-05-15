// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_UTILS_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_UTILS_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace high_efficiency {

// Returns whether |url| supports showing discard indicators
bool IsURLSupported(GURL url);

// Returns the discard reason if |contents| has been discarded
absl::optional<::mojom::LifecycleUnitDiscardReason> GetDiscardReason(
    content::WebContents* contents);

// Adds the given site to the discard exclusion list
void AddSiteToExceptionsList(PrefService* pref_service, std::string site);

}  // namespace high_efficiency

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_UTILS_H_
