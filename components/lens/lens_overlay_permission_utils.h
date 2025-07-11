// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_PERMISSION_UTILS_H_
#define COMPONENTS_LENS_LENS_OVERLAY_PERMISSION_UTILS_H_

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace lens {
namespace prefs {

// The possible values for the Lens Overlay enterprise policy. The value is an
// integer rather than a boolean to allow for additional states to be added in
// the future.
enum class LensOverlaySettingsPolicyValue {
  kEnabled = 0,
  kDisabled = 1,
};

// An integer setting indicating whether the Lens Overlay feature is enabled or
// disabled by the 'LensOverlaySettings' enterprise policy.
inline constexpr char kLensOverlaySettings[] =
    "lens.policy.lens_overlay_settings";

// A boolean indicating whether the whether the user has permitted sharing page
// screenshot with the Lens Overlay server.
inline constexpr char kLensSharingPageScreenshotEnabled[] =
    "lens.sharing_page_screenshot.enabled";

// A boolean indicating whether the whether the user has permitted sharing page
// content with the Lens Overlay server. Used for the contextual searchbox.
inline constexpr char kLensSharingPageContentEnabled[] =
    "lens.sharing_page_content.enabled";

// Registers the prefs used by the Lens Overlay.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace prefs

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the the page screenshot with the Lens Overlay server.
bool CanSharePageScreenshotWithLensOverlay(PrefService* pref_service);

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the the page context with the Lens Overlay server.
bool CanSharePageContentWithLensOverlay(PrefService* pref_service);

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the the page URL with the the Lens Overlay server. This can be through
// MSBB or accepting the CSB permission bubble, which informs the user about
// sharing the page URL.
bool CanSharePageURLWithLensOverlay(PrefService* pref_service);

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the information about the page title with the the Lens Overlay server.
// This can be through history sync or accepting the CSB permission bubble,
// which informs the user about sharing the page title.
bool CanSharePageTitleWithLensOverlay(syncer::SyncService* sync_service,
                                      PrefService* pref_service);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_PERMISSION_UTILS_H_
