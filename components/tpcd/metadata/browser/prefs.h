// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_BROWSER_PREFS_H_
#define COMPONENTS_TPCD_METADATA_BROWSER_PREFS_H_

class PrefRegistrySimple;

namespace tpcd::metadata {

namespace prefs {
// kCohorts references a dictionary of
// `content_settings::mojom::TpcdMetadataCohort` keyed by `MetadataEntry`. This
// is stored within the local state prefs store.
extern const char kCohorts[];
}  // namespace prefs

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace tpcd::metadata

#endif  // COMPONENTS_TPCD_METADATA_BROWSER_PREFS_H_
