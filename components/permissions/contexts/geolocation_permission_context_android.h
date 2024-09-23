// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

// The flow for geolocation permissions on Android needs to take into account
// the global geolocation settings so it differs from the desktop one. It
// works as follows.
// GeolocationPermissionContextAndroid::RequestPermission intercepts the flow
// and proceeds to check the system location.
// This will in fact check several possible settings
//     - The global system geolocation setting
//     - The Google location settings on pre KK devices
//     - An old internal Chrome setting on pre-JB MR1 devices
// With all that information it will decide if system location is enabled.
// If enabled, it proceeds with the per site flow via
// GeolocationPermissionContext (which will check per site permissions, create
// infobars, etc.).
//
// Otherwise the permission is already decided.
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/location/android/location_settings.h"
#include "components/location/android/location_settings_dialog_context.h"
#include "components/location/android/location_settings_dialog_outcome.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/permission_request_id.h"

namespace content {
class WebContents;
}

class GURL;
class PrefRegistrySimple;

namespace permissions {

class GeolocationPermissionContextAndroid
    : public GeolocationPermissionContext {
 public:
  // This enum is used in histograms, thus is append only. Do not re-order or
  // remove any entries, or add any except at the end.
  enum class LocationSettingsDialogBackOff {
    kNoBackOff,
    kOneWeek,
    kOneMonth,
    kThreeMonths,
    kCount,
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  GeolocationPermissionContextAndroid(
      content::BrowserContext* browser_context,
      std::unique_ptr<Delegate> delegate,
      bool is_regular_profile,
      std::unique_ptr<LocationSettings> settings_override_for_test = nullptr);

  GeolocationPermissionContextAndroid(
      const GeolocationPermissionContextAndroid&) = delete;
  GeolocationPermissionContextAndroid& operator=(
      const GeolocationPermissionContextAndroid&) = delete;

  ~GeolocationPermissionContextAndroid() override;

  static void AddDayOffsetForTesting(int days);

  // Overrides the LocationSettings object used to determine whether
  // system and Chrome-wide location permissions are enabled.
  void SetLocationSettingsForTesting(
      std::unique_ptr<LocationSettings> settings);

 private:
  // GeolocationPermissionContext:
  void RequestPermission(PermissionRequestData request_data,
                         BrowserPermissionCallback callback) override;
  void UserMadePermissionDecision(const PermissionRequestID& id,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  ContentSetting content_setting) override;
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override;
  content::PermissionResult UpdatePermissionStatusWithDeviceStatus(
      content::PermissionResult result,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  bool AlwaysIncludeDeviceStatus() const override;

  // Functions to handle back off for showing the Location Settings Dialog.
  std::string GetLocationSettingsBackOffLevelPref(bool is_default_search) const;
  std::string GetLocationSettingsNextShowPref(bool is_default_search) const;
  bool IsInLocationSettingsBackOff(bool is_default_search) const;
  void ResetLocationSettingsBackOff(bool is_default_search);
  void UpdateLocationSettingsBackOff(bool is_default_search);
  LocationSettingsDialogBackOff LocationSettingsBackOffLevel(
      bool is_default_search) const;

  // Returns whether location access is possible for the given origin. Ignores
  // Location Settings Dialog backoff, as the backoff is ignored if the user
  // will be prompted for permission.
  bool IsLocationAccessPossible(content::WebContents* web_contents,
                                const GURL& requesting_origin,
                                bool user_gesture);

  bool IsRequestingOriginDSE(const GURL& requesting_origin) const;

  void HandleUpdateAndroidPermissions(const PermissionRequestID& id,
                                      const GURL& requesting_frame_origin,
                                      const GURL& embedding_origin,
                                      BrowserPermissionCallback callback,
                                      bool permissions_updated);

  // Will return true if the location settings dialog will be shown for the
  // given origins. This is true if the location setting is off, the dialog can
  // be shown, any gesture requirements for the origin are met, and the dialog
  // is not being suppressed for backoff.
  bool CanShowLocationSettingsDialog(const GURL& requesting_origin,
                                     bool user_gesture,
                                     bool ignore_backoff) const;

  void OnLocationSettingsDialogShown(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool persist,
      ContentSetting content_setting,
      bool is_one_time,
      LocationSettingsDialogOutcome prompt_outcome);

  void FinishNotifyPermissionSet(const PermissionRequestID& id,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin,
                                 BrowserPermissionCallback callback,
                                 bool persist,
                                 ContentSetting content_setting,
                                 bool is_one_time);

  std::unique_ptr<LocationSettings> location_settings_;

  PermissionRequestID location_settings_dialog_request_id_;
  BrowserPermissionCallback location_settings_dialog_callback_;

  // Must be the last member, to ensure that it will be destroyed first, which
  // will invalidate weak pointers.
  base::WeakPtrFactory<GeolocationPermissionContextAndroid> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
