// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_sync/background_sync_controller_impl.h"

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "url/origin.h"

// static
const char BackgroundSyncControllerImpl::kFieldTrialName[] = "BackgroundSync";
const char BackgroundSyncControllerImpl::kDisabledParameterName[] = "disabled";
#if BUILDFLAG(IS_ANDROID)
const char BackgroundSyncControllerImpl::kRelyOnAndroidNetworkDetection[] =
    "rely_on_android_network_detection";
#endif
const char BackgroundSyncControllerImpl::kKeepBrowserAwakeParameterName[] =
    "keep_browser_awake_till_events_complete";
const char BackgroundSyncControllerImpl::kSkipPermissionsCheckParameterName[] =
    "skip_permissions_check_for_testing";
const char BackgroundSyncControllerImpl::kMaxAttemptsParameterName[] =
    "max_sync_attempts";
const char BackgroundSyncControllerImpl::
    kMaxAttemptsWithNotificationPermissionParameterName[] =
        "max_sync_attempts_with_notification_permission";
const char BackgroundSyncControllerImpl::kInitialRetryParameterName[] =
    "initial_retry_delay_sec";
const char BackgroundSyncControllerImpl::kRetryDelayFactorParameterName[] =
    "retry_delay_factor";
const char BackgroundSyncControllerImpl::kMinSyncRecoveryTimeName[] =
    "min_recovery_time_sec";
const char BackgroundSyncControllerImpl::kMaxSyncEventDurationName[] =
    "max_sync_event_duration_sec";
const char BackgroundSyncControllerImpl::kMinPeriodicSyncEventsInterval[] =
    "min_periodic_sync_events_interval_sec";

BackgroundSyncControllerImpl::BackgroundSyncControllerImpl(
    content::BrowserContext* browser_context,
    std::unique_ptr<background_sync::BackgroundSyncDelegate> delegate)
    : browser_context_(browser_context), delegate_(std::move(delegate)) {
  DCHECK(browser_context_);
  DCHECK(delegate_);

  background_sync_metrics_ =
      std::make_unique<BackgroundSyncMetrics>(delegate_.get());
  delegate_->GetHostContentSettingsMap()->AddObserver(this);
}

BackgroundSyncControllerImpl::~BackgroundSyncControllerImpl() = default;

void BackgroundSyncControllerImpl::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!content_type_set.Contains(ContentSettingsType::BACKGROUND_SYNC) &&
      !content_type_set.Contains(
          ContentSettingsType::PERIODIC_BACKGROUND_SYNC)) {
    return;
  }

  std::vector<url::Origin> affected_origins;
  for (const auto& origin : periodic_sync_origins_) {
    if (!IsContentSettingBlocked(origin))
      continue;

    auto* storage_partition = browser_context_->GetStoragePartitionForUrl(
        origin.GetURL(), /* can_create= */ false);
    if (!storage_partition)
      continue;

    auto* background_sync_context =
        storage_partition->GetBackgroundSyncContext();
    if (!background_sync_context)
      continue;

    background_sync_context->UnregisterPeriodicSyncForOrigin(origin);
    affected_origins.push_back(origin);
  }

  // Stop tracking affected origins.
  for (const auto& origin : affected_origins) {
    periodic_sync_origins_.erase(origin);
  }
}

void BackgroundSyncControllerImpl::GetParameterOverrides(
    content::BackgroundSyncParameters* parameters) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_ANDROID)
  if (delegate_->ShouldDisableBackgroundSync())
    parameters->disable = true;
#endif

  std::map<std::string, std::string> field_params;
  if (!base::GetFieldTrialParams(kFieldTrialName, &field_params)) {
    return;
  }

  if (base::EqualsCaseInsensitiveASCII(field_params[kDisabledParameterName],
                                       "true")) {
    parameters->disable = true;
  }

  if (base::EqualsCaseInsensitiveASCII(
          field_params[kKeepBrowserAwakeParameterName], "true")) {
    parameters->keep_browser_awake_till_events_complete = true;
  }

  if (base::EqualsCaseInsensitiveASCII(
          field_params[kSkipPermissionsCheckParameterName], "true")) {
    parameters->skip_permissions_check_for_testing = true;
  }

  if (base::Contains(field_params,
                     kMaxAttemptsWithNotificationPermissionParameterName)) {
    int max_attempts;
    if (base::StringToInt(
            field_params[kMaxAttemptsWithNotificationPermissionParameterName],
            &max_attempts)) {
      parameters->max_sync_attempts_with_notification_permission = max_attempts;
    }
  }

  if (base::Contains(field_params, kMaxAttemptsParameterName)) {
    int max_attempts;
    if (base::StringToInt(field_params[kMaxAttemptsParameterName],
                          &max_attempts)) {
      parameters->max_sync_attempts = max_attempts;
    }
  }

  if (base::Contains(field_params, kInitialRetryParameterName)) {
    int initial_retry_delay_sec;
    if (base::StringToInt(field_params[kInitialRetryParameterName],
                          &initial_retry_delay_sec)) {
      parameters->initial_retry_delay = base::Seconds(initial_retry_delay_sec);
    }
  }

  if (base::Contains(field_params, kRetryDelayFactorParameterName)) {
    int retry_delay_factor;
    if (base::StringToInt(field_params[kRetryDelayFactorParameterName],
                          &retry_delay_factor)) {
      parameters->retry_delay_factor = retry_delay_factor;
    }
  }

  if (base::Contains(field_params, kMinSyncRecoveryTimeName)) {
    int min_sync_recovery_time_sec;
    if (base::StringToInt(field_params[kMinSyncRecoveryTimeName],
                          &min_sync_recovery_time_sec)) {
      parameters->min_sync_recovery_time =
          base::Seconds(min_sync_recovery_time_sec);
    }
  }

  if (base::Contains(field_params, kMaxSyncEventDurationName)) {
    int max_sync_event_duration_sec;
    if (base::StringToInt(field_params[kMaxSyncEventDurationName],
                          &max_sync_event_duration_sec)) {
      parameters->max_sync_event_duration =
          base::Seconds(max_sync_event_duration_sec);
    }
  }

  if (base::Contains(field_params, kMinPeriodicSyncEventsInterval)) {
    int min_periodic_sync_events_interval_sec;
    if (base::StringToInt(field_params[kMinPeriodicSyncEventsInterval],
                          &min_periodic_sync_events_interval_sec)) {
      parameters->min_periodic_sync_events_interval =
          base::Seconds(min_periodic_sync_events_interval_sec);
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // Check if the delegate explicitly disabled this feature.
  if (delegate_->ShouldDisableAndroidNetworkDetection()) {
    parameters->rely_on_android_network_detection = false;
  } else if (base::Contains(field_params, kRelyOnAndroidNetworkDetection)) {
    if (base::EqualsCaseInsensitiveASCII(
            field_params[kRelyOnAndroidNetworkDetection], "true")) {
      parameters->rely_on_android_network_detection = true;
    }
  }
#endif

  return;
}

void BackgroundSyncControllerImpl::NotifyOneShotBackgroundSyncRegistered(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_->MaybeRecordOneShotSyncRegistrationEvent(
      origin, can_fire, is_reregistered);
}

void BackgroundSyncControllerImpl::NotifyPeriodicBackgroundSyncRegistered(
    const url::Origin& origin,
    int min_interval,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_->MaybeRecordPeriodicSyncRegistrationEvent(
      origin, min_interval, is_reregistered);
}

void BackgroundSyncControllerImpl::NotifyOneShotBackgroundSyncCompleted(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_->MaybeRecordOneShotSyncCompletionEvent(
      origin, status_code, num_attempts, max_attempts);
}

void BackgroundSyncControllerImpl::NotifyPeriodicBackgroundSyncCompleted(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_->MaybeRecordPeriodicSyncEventCompletion(
      origin, status_code, num_attempts, max_attempts);
}

void BackgroundSyncControllerImpl::ScheduleBrowserWakeUpWithDelay(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (delegate_->IsProfileOffTheRecord())
    return;

#if BUILDFLAG(IS_ANDROID)
  delegate_->ScheduleBrowserWakeUpWithDelay(sync_type, delay);
#endif
}

void BackgroundSyncControllerImpl::CancelBrowserWakeup(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (delegate_->IsProfileOffTheRecord())
    return;

#if BUILDFLAG(IS_ANDROID)
  delegate_->CancelBrowserWakeup(sync_type);
#endif
}

base::TimeDelta BackgroundSyncControllerImpl::SnapToMaxOriginFrequency(
    int64_t min_interval,
    int64_t min_gap_for_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK_GE(min_gap_for_origin, 0);
  DCHECK_GE(min_interval, 0);

  if (min_interval < min_gap_for_origin)
    return base::Milliseconds(min_gap_for_origin);
  if (min_interval % min_gap_for_origin == 0)
    return base::Milliseconds(min_interval);
  return base::Milliseconds((min_interval / min_gap_for_origin + 1) *
                            min_gap_for_origin);
}

base::TimeDelta BackgroundSyncControllerImpl::ApplyMinGapForOrigin(
    base::TimeDelta delay,
    base::TimeDelta time_till_next_scheduled_event_for_origin,
    base::TimeDelta min_gap_for_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (time_till_next_scheduled_event_for_origin.is_max())
    return delay;

  if (delay <= time_till_next_scheduled_event_for_origin - min_gap_for_origin)
    return delay;

  if (delay <= time_till_next_scheduled_event_for_origin)
    return time_till_next_scheduled_event_for_origin;

  if (delay <= time_till_next_scheduled_event_for_origin + min_gap_for_origin)
    return time_till_next_scheduled_event_for_origin + min_gap_for_origin;

  return delay;
}

bool BackgroundSyncControllerImpl::IsContentSettingBlocked(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* host_content_settings_map = delegate_->GetHostContentSettingsMap();
  DCHECK(host_content_settings_map);

  auto url = origin.GetURL();
  return CONTENT_SETTING_ALLOW != host_content_settings_map->GetContentSetting(
                                      /* primary_url= */ url,
                                      /* secondary_url= */ url,
                                      ContentSettingsType::BACKGROUND_SYNC);
}

void BackgroundSyncControllerImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->GetHostContentSettingsMap()->RemoveObserver(this);
  delegate_->Shutdown();
}

base::TimeDelta BackgroundSyncControllerImpl::GetNextEventDelay(
    const content::BackgroundSyncRegistration& registration,
    content::BackgroundSyncParameters* parameters,
    base::TimeDelta time_till_soonest_scheduled_event_for_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(parameters);

  int num_attempts = registration.num_attempts();

  if (!num_attempts) {
    // First attempt.
    switch (registration.sync_type()) {
      case blink::mojom::BackgroundSyncType::ONE_SHOT:
        return base::TimeDelta();
      case blink::mojom::BackgroundSyncType::PERIODIC:
        int site_engagement_factor =
            delegate_->GetSiteEngagementPenalty(registration.origin().GetURL());
        if (!site_engagement_factor)
          return base::TimeDelta::Max();

        int64_t effective_gap_ms =
            site_engagement_factor *
            parameters->min_periodic_sync_events_interval.InMilliseconds();
        return ApplyMinGapForOrigin(
            SnapToMaxOriginFrequency(registration.options()->min_interval,
                                     effective_gap_ms),
            time_till_soonest_scheduled_event_for_origin,
            parameters->min_periodic_sync_events_interval);
    }
  }

  // After a sync event has been fired.
  DCHECK_LT(num_attempts, parameters->max_sync_attempts);
  return parameters->initial_retry_delay *
         pow(parameters->retry_delay_factor, num_attempts - 1);
}

std::unique_ptr<content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
BackgroundSyncControllerImpl::CreateBackgroundSyncEventKeepAlive() {
#if BUILDFLAG(IS_ANDROID)
  // Not needed on Android.
  return nullptr;
#else
  return delegate_->CreateBackgroundSyncEventKeepAlive();
#endif
}

void BackgroundSyncControllerImpl::NoteSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  delegate_->NoteSuspendedPeriodicSyncOrigins(std::move(suspended_origins));
}

void BackgroundSyncControllerImpl::NoteRegisteredPeriodicSyncOrigins(
    std::set<url::Origin> registered_origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (auto& origin : registered_origins)
    periodic_sync_origins_.insert(std::move(origin));
}

void BackgroundSyncControllerImpl::AddToTrackedOrigins(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  periodic_sync_origins_.insert(origin);
}

void BackgroundSyncControllerImpl::RemoveFromTrackedOrigins(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  periodic_sync_origins_.erase(origin);
}
