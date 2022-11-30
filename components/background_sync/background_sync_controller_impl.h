// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_
#define COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/background_sync_controller.h"

#include <stdint.h>

#include <set>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/background_sync/background_sync_delegate.h"
#include "components/background_sync/background_sync_metrics.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom-forward.h"

namespace content {
struct BackgroundSyncParameters;
class BrowserContext;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class BackgroundSyncControllerImpl : public content::BackgroundSyncController,
                                     public KeyedService,
                                     public content_settings::Observer {
 public:
  static const char kFieldTrialName[];
  static const char kDisabledParameterName[];
  static const char kKeepBrowserAwakeParameterName[];
  static const char kSkipPermissionsCheckParameterName[];
  static const char kMaxAttemptsParameterName[];
  static const char kRelyOnAndroidNetworkDetection[];
  static const char kMaxAttemptsWithNotificationPermissionParameterName[];
  static const char kInitialRetryParameterName[];
  static const char kRetryDelayFactorParameterName[];
  static const char kMinSyncRecoveryTimeName[];
  static const char kMaxSyncEventDurationName[];
  static const char kMinPeriodicSyncEventsInterval[];

  BackgroundSyncControllerImpl(
      content::BrowserContext* browser_context,
      std::unique_ptr<background_sync::BackgroundSyncDelegate> delegate);

  BackgroundSyncControllerImpl(const BackgroundSyncControllerImpl&) = delete;
  BackgroundSyncControllerImpl& operator=(const BackgroundSyncControllerImpl&) =
      delete;

  ~BackgroundSyncControllerImpl() override;

  // content::BackgroundSyncController overrides.
  void GetParameterOverrides(
      content::BackgroundSyncParameters* parameters) override;
  void NotifyOneShotBackgroundSyncRegistered(const url::Origin& origin,
                                             bool can_fire,
                                             bool is_reregistered) override;
  void NotifyPeriodicBackgroundSyncRegistered(const url::Origin& origin,
                                              int min_interval,
                                              bool is_reregistered) override;
  void NotifyOneShotBackgroundSyncCompleted(
      const url::Origin& origin,
      blink::ServiceWorkerStatusCode status_code,
      int num_attempts,
      int max_attempts) override;
  void NotifyPeriodicBackgroundSyncCompleted(
      const url::Origin& origin,
      blink::ServiceWorkerStatusCode status_code,
      int num_attempts,
      int max_attempts) override;
  void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay) override;
  void CancelBrowserWakeup(blink::mojom::BackgroundSyncType sync_type) override;

  base::TimeDelta GetNextEventDelay(
      const content::BackgroundSyncRegistration& registration,
      content::BackgroundSyncParameters* parameters,
      base::TimeDelta time_till_soonest_scheduled_event_for_origin) override;

  std::unique_ptr<BackgroundSyncEventKeepAlive>
  CreateBackgroundSyncEventKeepAlive() override;
  void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) override;
  void NoteRegisteredPeriodicSyncOrigins(
      std::set<url::Origin> registered_origins) override;
  void AddToTrackedOrigins(const url::Origin& origin) override;
  void RemoveFromTrackedOrigins(const url::Origin& origin) override;

  // content_settings::Observer overrides.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  bool IsOriginTracked(const url::Origin& origin) {
    return periodic_sync_origins_.find(origin) != periodic_sync_origins_.end();
  }

 private:
  // Once we've identified the minimum number of hours between each periodicsync
  // event for an origin, every delay calculated for the origin should be a
  // multiple of the same.
  base::TimeDelta SnapToMaxOriginFrequency(int64_t min_interval,
                                           int64_t min_gap_for_origin);

  // Returns an updated delay for a Periodic Background Sync registration -- one
  // that ensures the |min_gap_for_origin|.
  base::TimeDelta ApplyMinGapForOrigin(
      base::TimeDelta delay,
      base::TimeDelta time_till_next_scheduled_event_for_origin,
      base::TimeDelta min_gap_for_origin);

  bool IsContentSettingBlocked(const url::Origin& origin);

  // KeyedService implementation.
  void Shutdown() override;

  raw_ptr<content::BrowserContext> browser_context_;

  std::unique_ptr<background_sync::BackgroundSyncDelegate> delegate_;
  std::unique_ptr<BackgroundSyncMetrics> background_sync_metrics_;

  std::set<url::Origin> periodic_sync_origins_;
};

#endif  // COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_
