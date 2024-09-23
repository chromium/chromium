// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PROFILE_INTERACTION_MANAGER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PROFILE_INTERACTION_MANAGER_H_

#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"

namespace subresource_filter {
enum class ActivationDecision;

namespace mojom {
enum class ActivationLevel;
}  // namespace mojom

}  // namespace subresource_filter

namespace content {
class NavigationHandle;
}  // namespace content

namespace fingerprinting_protection_filter {

// Class that manages interaction between the per-navigation/per-page
// subresource filter objects (i.e., the throttles and throttle manager) and
// the per-profile objects (e.g., content settings).
class ProfileInteractionManager {
 public:
  explicit ProfileInteractionManager(
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      PrefService* prefs);
  virtual ~ProfileInteractionManager();

  ProfileInteractionManager& operator=(const ProfileInteractionManager&) =
      delete;

  virtual subresource_filter::mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      subresource_filter::mojom::ActivationLevel initial_activation_level,
      subresource_filter::ActivationDecision* decision);
  virtual content_settings::SettingSource GetTrackingProtectionSettingSource(
      const GURL& url);

 private:
  // TODO(https://crbug.com/40280666): Triage dangling pointers.
  raw_ptr<privacy_sandbox::TrackingProtectionSettings, DanglingUntriaged>
      tracking_protection_settings_;
  raw_ptr<PrefService, DanglingUntriaged> prefs_;
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PROFILE_INTERACTION_MANAGER_H_
