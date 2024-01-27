// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_H_
#define COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_H_

#include <optional>
#include <set>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/background_sync_controller.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom-forward.h"
#include "url/origin.h"

class HostContentSettingsMap;

class GURL;

namespace background_sync {

// Allows the component embedder to override the behavior of Background Sync
// component.
class BackgroundSyncDelegate {
 public:
  virtual ~BackgroundSyncDelegate() = default;

#if !BUILDFLAG(IS_ANDROID)
  // Keeps the browser and profile alive to allow a one-shot Background Sync
  // registration to finish firing one sync event.
  virtual std::unique_ptr<
      content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
  CreateBackgroundSyncEventKeepAlive() = 0;
#endif

  // Gets the source_ID to log the UKM event for, and calls |callback| with that
  // source_id, or with std::nullopt if UKM recording is not allowed.
  virtual void GetUkmSourceId(
      const url::Origin& origin,
      base::OnceCallback<void(std::optional<ukm::SourceId>)> callback) = 0;

  // Handles browser shutdown.
  virtual void Shutdown() = 0;

  // Returns the content settings map.
  virtual HostContentSettingsMap* GetHostContentSettingsMap() = 0;

  // Returns true if the profile associated with the delegate is off-the-record.
  virtual bool IsProfileOffTheRecord() = 0;

  // Notes the origins for which Periodic Background Sync is suspended.
  virtual void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) = 0;

  // Gets the site engagement penalty to add to the Periodic Background Sync
  // interval for the origin corresponding to |url|.
  // The site engagement penalty is inversely proportional to the engagement
  // level. The lower the engagement levels with the site, the less often
  // periodic sync events will be fired.
  // Returns 0 if the engagement level is blink::mojom::EngagementLevel::NONE.
  virtual int GetSiteEngagementPenalty(const GURL& url) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Schedules the browser to be woken up when the device is online to process
  // registrations of type |sync| after a minimum delay |delay|.
  virtual void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay) = 0;

  // Cancels browser wakeup for registrations of type |sync_type|.
  virtual void CancelBrowserWakeup(
      blink::mojom::BackgroundSyncType sync_type) = 0;

  // Whether Background Sync should be disabled.
  virtual bool ShouldDisableBackgroundSync() = 0;

  // Whether to disable Android network detection for connectivity checks.
  virtual bool ShouldDisableAndroidNetworkDetection() = 0;
#endif
};

}  // namespace background_sync

#endif  // COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_H_
