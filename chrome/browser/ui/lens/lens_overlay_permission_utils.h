// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_PERMISSION_UTILS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_PERMISSION_UTILS_H_

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace lens {

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the the page URL with the the Lens Overlay server.
bool CanSharePageURLWithLensOverlay(PrefService* pref_service);

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the information about the page title with the the Lens Overlay server.
bool CanSharePageTitleWithLensOverlay(syncer::SyncService* sync_service);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_PERMISSION_UTILS_H_
