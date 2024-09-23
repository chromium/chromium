// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_CONSENT_HELPER_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_CONSENT_HELPER_H_

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace page_image_service {

enum class PageImageServiceConsentStatus;

// Helper class that observes SyncService for when it is appropriate to fetch
// images for synced entities that have been viewed in the past.
class ImageServiceConsentHelper : public syncer::SyncServiceObserver {
 public:
  ImageServiceConsentHelper(syncer::SyncService* sync_service,
                            syncer::DataType data_type);
  ~ImageServiceConsentHelper() override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  // If Sync downloads for `data_type_` have already been initialized, this
  // method calls `callback` synchronously with the result. If not, it will hold
  // the request up until the timeout for the consent helper to initialize.
  void EnqueueRequest(
      base::OnceCallback<void(PageImageServiceConsentStatus)> callback,
      mojom::ClientId client_id);

 private:
  // Returns whether it is appropriate to fetch images for synced entities of
  // `data_type_`. Will return nullopt if Sync Service is not ready yet.
  std::optional<bool> GetConsentStatus();

  // Run periodically to sweep away old queued requests.
  void OnTimeoutExpired();

  // The sync service `this` is observing.
  raw_ptr<syncer::SyncService> sync_service_;

  // The data type `this` pertains to.
  const syncer::DataType data_type_;

  // Timer used to periodically process unanswered enqueued requests, and
  // respond to them in the negative.
  base::OneShotTimer request_processing_timer_;

  // Requests waiting for the consent throttle to initialize. Requests are
  // stored in the queue in order of their arrival.
  std::vector<std::pair<base::OnceCallback<void(PageImageServiceConsentStatus)>,
                        mojom::ClientId>>
      enqueued_request_callbacks_;

  // The duration to wait before returning some answer back for the request.
  const base::TimeDelta timeout_duration_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ImageServiceConsentHelper> weak_ptr_factory_{this};
};

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_CONSENT_HELPER_H_
