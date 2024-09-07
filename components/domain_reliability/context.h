// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "net/base/isolation_info.h"

class GURL;

namespace base {
class Value;
}

namespace domain_reliability {

class DomainReliabilityDispatcher;
class DomainReliabilityUploader;
class MockableTime;

// The per-domain context for the Domain Reliability client; includes the
// domain's config and beacon queue.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityContext {
 public:
  // Maximum upload depth to schedule an upload. If a beacon is based on a more
  // deeply nested upload, it will be reported eventually, but will not itself
  // trigger a new upload.
  static const int kMaxUploadDepthToSchedule;

  using UploadAllowedCallback =
      base::RepeatingCallback<void(const url::Origin&,
                                   base::OnceCallback<void(bool)>)>;

  DomainReliabilityContext(
      const MockableTime* time,
      const DomainReliabilityScheduler::Params& scheduler_params,
      const std::string& upload_reporter_string,
      const base::TimeTicks* last_network_change_time,
      const UploadAllowedCallback& upload_allowed_callback,
      DomainReliabilityDispatcher* dispatcher,
      DomainReliabilityUploader* uploader,
      std::unique_ptr<const DomainReliabilityConfig> config);

  DomainReliabilityContext(const DomainReliabilityContext&) = delete;
  DomainReliabilityContext& operator=(const DomainReliabilityContext&) = delete;

  ~DomainReliabilityContext();

  // Notifies the context of a beacon on its domain(s); may or may not save the
  // actual beacon to be uploaded, depending on the sample rates in the config,
  // but will increment one of the request counters in any case.
  void OnBeacon(std::unique_ptr<DomainReliabilityBeacon> beacon);

  // Called to clear browsing data, since beacons are like browsing history.
  void ClearBeacons();

  // Gets the beacons queued for upload in this context. `*beacons_out` will be
  // cleared and filled with pointers to the beacons; the pointers remain valid
  // as long as no other requests are reported to the DomainReliabilityMonitor.
  void GetQueuedBeaconsForTesting(
      std::vector<const DomainReliabilityBeacon*>* beacons_out) const;

  const DomainReliabilityConfig& config() const { return *config_.get(); }

  // Maximum number of beacons queued per context; if more than this many are
  // queued; the oldest beacons will be removed.
  static const size_t kMaxQueuedBeacons;

 private:
  void ScheduleUpload(base::TimeDelta min_delay, base::TimeDelta max_delay);
  void CallUploadAllowedCallback();
  void OnUploadAllowedCallbackComplete(bool allowed);
  void StartUpload();
  void OnUploadComplete(const DomainReliabilityUploader::UploadResult& result);

  // Creates a report from all beacons associated with
  // `uploading_beacons_network_anonymization_key_`. Updates
  // `uploading_beacons_size_`.
  base::Value CreateReport(base::TimeTicks upload_time,
                           const GURL& collector_url,
                           int* max_beacon_depth_out);

  // Uses the state remembered by `MarkUpload` to remove successfully uploaded
  // data but keep beacons and request counts added after the upload started.
  void CommitUpload();

  void RollbackUpload();

  // Finds and removes the oldest beacon. DCHECKs if there is none. (Called
  // when there are too many beacons queued.)
  void RemoveOldestBeacon();

  void RemoveExpiredBeacons();

  // Gets the minimum upload depth of all entries in |beacons_|.
  int GetMinBeaconUploadDepth() const;

  std::unique_ptr<const DomainReliabilityConfig> config_;
  raw_ptr<const MockableTime> time_;
  const raw_ref<const std::string> upload_reporter_string_;
  DomainReliabilityScheduler scheduler_;
  raw_ptr<DomainReliabilityDispatcher> dispatcher_;
  raw_ptr<DomainReliabilityUploader> uploader_;

  std::list<std::unique_ptr<DomainReliabilityBeacon>> beacons_;

  size_t uploading_beacons_size_;
  // The IsolationInfo associated with the beacons being uploaded. The first
  // `uploading_beacons_size_` beacons that have NIK equal to the NIK of
  // `uploading_beacons_isolation_info_` are currently being uploaded. It's
  // possible for this number to be 0 when there's still an active upload if
  // all currently uploading beacons have been evicted.
  //
  // Note that requests technically expose top level origins, which may be
  // different than the top-level site used by the NetworkIsolationKey to
  // partition uploads. This shouldn't affect anything in practice (e.g., cookie
  // blocking), since uploads will only ever use uncredentialed requests.
  net::IsolationInfo uploading_beacons_isolation_info_;

  base::TimeTicks upload_time_;
  base::TimeTicks last_upload_time_;
  // The last network change time is not tracked per-context, so this is a
  // pointer to that value in a wider (e.g. per-Monitor or unittest) scope.
  raw_ptr<const base::TimeTicks> last_network_change_time_;
  const raw_ref<const UploadAllowedCallback> upload_allowed_callback_;

  base::WeakPtrFactory<DomainReliabilityContext> weak_factory_{this};
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_
