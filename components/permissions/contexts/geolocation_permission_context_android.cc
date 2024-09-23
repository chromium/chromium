// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context_android.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/location/android/location_settings.h"
#include "components/location/android/location_settings_impl.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/android/permissions_reprompt_controller_android.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace permissions {
namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

int g_day_offset_for_testing = 0;

base::Time GetTimeNow() {
  return base::Time::Now() + base::Days(g_day_offset_for_testing);
}

// These values are recorded in histograms. Entries should not be renumbered and
// numeric values should never be reused.
enum class AndroidLocationPermissionState {
  kNoAccess = 0,
  kAccessCoarse = 1,
  kAccessFine = 2,
  kMaxValue = kAccessFine,
};

void RecordUmaPermissionState(AndroidLocationPermissionState state) {
  base::UmaHistogramEnumeration("Geolocation.Android.LocationPermissionState",
                                state);
}

}  // namespace

// static
void GeolocationPermissionContextAndroid::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kLocationSettingsBackoffLevelDSE, 0);
  registry->RegisterIntegerPref(prefs::kLocationSettingsBackoffLevelDefault, 0);
  registry->RegisterInt64Pref(prefs::kLocationSettingsNextShowDSE, 0);
  registry->RegisterInt64Pref(prefs::kLocationSettingsNextShowDefault, 0);
}

GeolocationPermissionContextAndroid::GeolocationPermissionContextAndroid(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate,
    bool is_regular_profile,
    std::unique_ptr<LocationSettings> settings_override_for_test)
    : GeolocationPermissionContext(browser_context, std::move(delegate)),
      location_settings_(std::move(settings_override_for_test)),
      location_settings_dialog_request_id_(
          content::GlobalRenderFrameHostId(0, 0),
          PermissionRequestID::RequestLocalId()) {
  if (!location_settings_) {
    location_settings_ = std::make_unique<LocationSettingsImpl>();
  }
  if (is_regular_profile) {
    // Record the initial system permission state.
    if (location_settings_->HasAndroidFineLocationPermission()) {
      RecordUmaPermissionState(AndroidLocationPermissionState::kAccessFine);
    } else if (location_settings_->HasAndroidLocationPermission()) {
      RecordUmaPermissionState(AndroidLocationPermissionState::kAccessCoarse);
    } else {
      RecordUmaPermissionState(AndroidLocationPermissionState::kNoAccess);
    }
  }
}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() =
    default;

// static
void GeolocationPermissionContextAndroid::AddDayOffsetForTesting(int days) {
  g_day_offset_for_testing += days;
}

void GeolocationPermissionContextAndroid::RequestPermission(
    PermissionRequestData request_data,
    BrowserPermissionCallback callback) {
  content::RenderFrameHost* const render_frame_host =
      content::RenderFrameHost::FromID(
          request_data.id.global_render_frame_host_id());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  request_data.embedding_origin = PermissionUtil::GetLastCommittedOriginAsURL(
      render_frame_host->GetMainFrame());

  if (!IsLocationAccessPossible(web_contents, request_data.requesting_origin,
                                request_data.user_gesture)) {
    NotifyPermissionSet(request_data.id, request_data.requesting_origin,
                        request_data.embedding_origin, std::move(callback),
                        /*persist=*/false, CONTENT_SETTING_BLOCK,
                        /*is_one_time=*/false, /*is_final_decision=*/true);
    return;
  }

  DCHECK(render_frame_host);
  PermissionStatus status =
      GeolocationPermissionContext::GetPermissionStatus(
          render_frame_host, request_data.requesting_origin,
          request_data.embedding_origin)
          .status;
  if (status == PermissionStatus::GRANTED &&
      ShouldRepromptUserForPermissions(web_contents,
                                       {ContentSettingsType::GEOLOCATION}) ==
          PermissionRepromptState::kShow) {
    permissions::PermissionsRepromptControllerAndroid::CreateForWebContents(
        web_contents);
    permissions::PermissionsRepromptControllerAndroid::FromWebContents(
        web_contents)
        ->RepromptPermissionRequest(
            {ContentSettingsType::GEOLOCATION}, content_settings_type(),
            base::BindOnce(&GeolocationPermissionContextAndroid::
                               HandleUpdateAndroidPermissions,
                           weak_factory_.GetWeakPtr(), request_data.id,
                           request_data.requesting_origin,
                           request_data.embedding_origin, std::move(callback)));
    return;
  }

  GeolocationPermissionContext::RequestPermission(std::move(request_data),
                                                  std::move(callback));
}

void GeolocationPermissionContextAndroid::UserMadePermissionDecision(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  // If the user has accepted geolocation, reset the location settings dialog
  // backoff.
  if (content_setting == CONTENT_SETTING_ALLOW)
    ResetLocationSettingsBackOff(IsRequestingOriginDSE(requesting_origin));
}

void GeolocationPermissionContextAndroid::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK(is_final_decision);

  bool is_default_search = IsRequestingOriginDSE(requesting_origin);
  if (content_setting == CONTENT_SETTING_ALLOW &&
      !location_settings_->IsSystemLocationSettingEnabled()) {
    // There is no need to check CanShowLocationSettingsDialog here again, as it
    // must have been checked to get this far. But, the backoff will not have
    // been checked, so check that. Backoff isn't checked earlier because if the
    // content setting is ASK the backoff should be ignored. However if we get
    // here and the content setting was ASK, the user must have accepted which
    // would reset the backoff.
    if (IsInLocationSettingsBackOff(is_default_search)) {
      FinishNotifyPermissionSet(id, requesting_origin, embedding_origin,
                                std::move(callback), false /* persist */,
                                CONTENT_SETTING_BLOCK, is_one_time);
      return;
    }

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(id.global_render_frame_host_id()));
    if (!web_contents)
      return;

    // Only show the location settings dialog if the tab for |web_contents| is
    // user-interactable (i.e. is the current tab, and Chrome is active and not
    // in tab-switching mode) and we're not already showing the LSD. The latter
    // case can occur in split-screen multi-window.
    if (!delegate_->IsInteractable(web_contents) ||
        !location_settings_dialog_callback_.is_null()) {
      FinishNotifyPermissionSet(id, requesting_origin, embedding_origin,
                                std::move(callback), false /* persist */,
                                CONTENT_SETTING_BLOCK, is_one_time);
      return;
    }

    location_settings_dialog_request_id_ = id;
    location_settings_dialog_callback_ = std::move(callback);
    location_settings_->PromptToEnableSystemLocationSetting(
        is_default_search ? SEARCH : DEFAULT,
        web_contents->GetTopLevelNativeWindow(),
        base::BindOnce(
            &GeolocationPermissionContextAndroid::OnLocationSettingsDialogShown,
            weak_factory_.GetWeakPtr(), requesting_origin, embedding_origin,
            persist, content_setting, is_one_time));
    return;
  }

  FinishNotifyPermissionSet(id, requesting_origin, embedding_origin,
                            std::move(callback), persist, content_setting,
                            is_one_time);
}

content::PermissionResult
GeolocationPermissionContextAndroid::UpdatePermissionStatusWithDeviceStatus(
    content::PermissionResult result,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (result.status != PermissionStatus::DENIED) {
    if (!location_settings_->IsSystemLocationSettingEnabled()) {
      // As this is returning the status for possible future permission
      // requests, whose gesture status is unknown, pretend there is a user
      // gesture here. If there is a possibility of PROMPT (i.e. if there is a
      // user gesture attached to the later request) that should be returned,
      // not BLOCK.
      // If the permission is in the ASK state, backoff is ignored. Permission
      // prompts are shown regardless of backoff, if the location is off and the
      // LSD can be shown, as permission prompts are less annoying than the
      // modal LSD, and if the user accepts the permission prompt the LSD
      // backoff will be reset.
      if (CanShowLocationSettingsDialog(
              requesting_origin, true /* user_gesture */,
              result.status == PermissionStatus::ASK /* ignore_backoff */)) {
        result.status = PermissionStatus::ASK;
      } else {
        result.status = PermissionStatus::DENIED;
      }
      result.source = content::PermissionStatusSource::UNSPECIFIED;
    }

    if (result.status != PermissionStatus::DENIED &&
        !location_settings_->HasAndroidLocationPermission()) {
      // TODO(benwells): plumb through the RFH and use the associated
      // WebContents to check that the android location can be prompted for.
      result.status = PermissionStatus::ASK;
      result.source = content::PermissionStatusSource::UNSPECIFIED;
    }
  }

  return result;
}

bool GeolocationPermissionContextAndroid::AlwaysIncludeDeviceStatus() const {
  return true;
}

std::string
GeolocationPermissionContextAndroid::GetLocationSettingsBackOffLevelPref(
    bool is_default_search) const {
  return is_default_search ? prefs::kLocationSettingsBackoffLevelDSE
                           : prefs::kLocationSettingsBackoffLevelDefault;
}

std::string
GeolocationPermissionContextAndroid::GetLocationSettingsNextShowPref(
    bool is_default_search) const {
  return is_default_search ? prefs::kLocationSettingsNextShowDSE
                           : prefs::kLocationSettingsNextShowDefault;
}

bool GeolocationPermissionContextAndroid::IsInLocationSettingsBackOff(
    bool is_default_search) const {
  base::Time next_show = base::Time::FromInternalValue(
      delegate_->GetPrefs(browser_context())
          ->GetInt64(GetLocationSettingsNextShowPref(is_default_search)));

  return GetTimeNow() < next_show;
}

void GeolocationPermissionContextAndroid::ResetLocationSettingsBackOff(
    bool is_default_search) {
  PrefService* prefs = delegate_->GetPrefs(browser_context());
  prefs->ClearPref(GetLocationSettingsNextShowPref(is_default_search));
  prefs->ClearPref(GetLocationSettingsBackOffLevelPref(is_default_search));
}

void GeolocationPermissionContextAndroid::UpdateLocationSettingsBackOff(
    bool is_default_search) {
  LocationSettingsDialogBackOff backoff_level =
      LocationSettingsBackOffLevel(is_default_search);

  base::Time next_show = GetTimeNow();
  switch (backoff_level) {
    case LocationSettingsDialogBackOff::kNoBackOff:
      backoff_level = LocationSettingsDialogBackOff::kOneWeek;
      next_show += base::Days(7);
      break;
    case LocationSettingsDialogBackOff::kOneWeek:
      backoff_level = LocationSettingsDialogBackOff::kOneMonth;
      next_show += base::Days(30);
      break;
    case LocationSettingsDialogBackOff::kOneMonth:
      backoff_level = LocationSettingsDialogBackOff::kThreeMonths;
      [[fallthrough]];
    case LocationSettingsDialogBackOff::kThreeMonths:
      next_show += base::Days(90);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  PrefService* prefs = delegate_->GetPrefs(browser_context());
  prefs->SetInteger(GetLocationSettingsBackOffLevelPref(is_default_search),
                    static_cast<int>(backoff_level));
  prefs->SetInt64(GetLocationSettingsNextShowPref(is_default_search),
                  next_show.ToInternalValue());
}

GeolocationPermissionContextAndroid::LocationSettingsDialogBackOff
GeolocationPermissionContextAndroid::LocationSettingsBackOffLevel(
    bool is_default_search) const {
  PrefService* prefs = delegate_->GetPrefs(browser_context());
  int int_backoff =
      prefs->GetInteger(GetLocationSettingsBackOffLevelPref(is_default_search));
  return static_cast<LocationSettingsDialogBackOff>(int_backoff);
}

bool GeolocationPermissionContextAndroid::IsLocationAccessPossible(
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    bool user_gesture) {
  return (location_settings_->HasAndroidLocationPermission() ||
          location_settings_->CanPromptForAndroidLocationPermission(
              web_contents->GetTopLevelNativeWindow())) &&
         (location_settings_->IsSystemLocationSettingEnabled() ||
          CanShowLocationSettingsDialog(requesting_origin, user_gesture,
                                        true /* ignore_backoff */));
}

bool GeolocationPermissionContextAndroid::IsRequestingOriginDSE(
    const GURL& requesting_origin) const {
  return delegate_->IsRequestingOriginDSE(browser_context(), requesting_origin);
}

void GeolocationPermissionContextAndroid::HandleUpdateAndroidPermissions(
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool permissions_updated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ContentSetting new_setting =
      permissions_updated ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;

  NotifyPermissionSet(id, requesting_frame_origin, embedding_origin,
                      std::move(callback), false /* persist */, new_setting,
                      /*is_one_time=*/false, /*is_final_decision=*/true);
}

bool GeolocationPermissionContextAndroid::CanShowLocationSettingsDialog(
    const GURL& requesting_origin,
    bool user_gesture,
    bool ignore_backoff) const {
  bool is_default_search = IsRequestingOriginDSE(requesting_origin);
  // If this isn't the default search engine, a gesture is needed.
  if (!is_default_search && !user_gesture) {
    return false;
  }

  if (!ignore_backoff && IsInLocationSettingsBackOff(is_default_search))
    return false;

  return location_settings_->CanPromptToEnableSystemLocationSetting();
}

void GeolocationPermissionContextAndroid::OnLocationSettingsDialogShown(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    LocationSettingsDialogOutcome prompt_outcome) {
  bool is_default_search = IsRequestingOriginDSE(requesting_origin);
  if (prompt_outcome == GRANTED) {
    ResetLocationSettingsBackOff(is_default_search);
  } else {
    UpdateLocationSettingsBackOff(is_default_search);
    content_setting = CONTENT_SETTING_BLOCK;
    persist = false;
  }

  // If the permission was cancelled while the LSD was up, the callback has
  // already been dropped.
  if (!location_settings_dialog_callback_)
    return;

  FinishNotifyPermissionSet(location_settings_dialog_request_id_,
                            requesting_origin, embedding_origin,
                            std::move(location_settings_dialog_callback_),
                            persist, content_setting, is_one_time);

  location_settings_dialog_request_id_ =
      PermissionRequestID(content::GlobalRenderFrameHostId(0, 0),
                          PermissionRequestID::RequestLocalId());
}

void GeolocationPermissionContextAndroid::FinishNotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time) {
  GeolocationPermissionContext::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, is_one_time, /*is_final_decision=*/true);
}

void GeolocationPermissionContextAndroid::SetLocationSettingsForTesting(
    std::unique_ptr<LocationSettings> settings) {
  location_settings_ = std::move(settings);
}

}  // namespace permissions
