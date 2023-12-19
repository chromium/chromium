// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace segmentation_platform {

class UkmDatabase;
class UrlSignalHandler;
class UkmConfig;
class UkmObserver;

// Manages ownership and lifetime of all UKM related classes, like database and
// observer. There is only one manager per browser process. Created before
// profile initialization and destroyed after all profiles are destroyed. The
// database, observer and signal handler can have different lifetimes, see
// comments below.
class UkmDataManager {
 public:
  UkmDataManager() = default;
  virtual ~UkmDataManager() = default;

  UkmDataManager(const UkmDataManager&) = delete;
  UkmDataManager& operator=(const UkmDataManager&) = delete;

  // Initializes UKM database and the observer of all UKM events.
  virtual void Initialize(const base::FilePath& database_path,
                          bool in_memory) = 0;

  virtual void StartObservation(UkmObserver* ukm_observer) = 0;

  // Returns true when UKM engine is usable. If false, then UKM based engine is
  // disabled and this class is a no-op. UkmObserver, UrlSignalHandler and
  // UkmDatabase are not created and are unusable when this method returns
  // false.
  virtual bool IsUkmEngineEnabled() = 0;

  // Can be called at any time, starts observing UKM with the given config. This
  // must be called after the Initialize() call.
  virtual void StartObservingUkm(const UkmConfig& config) = 0;

  // Pauses or resumes observation of UKM, can be called any time, irrespective
  // of UKM observer's lifetime, similar to StartObservingUkm().
  virtual void PauseOrResumeObservation(bool pause) = 0;

  // Get URL signal handler. The signal handler is safe to use as long as data
  // manager is alive, so until after all profiles are destroyed.
  virtual UrlSignalHandler* GetOrCreateUrlHandler() = 0;

  // Get UKM database. The database is safe to use as long as data manager is
  // alive, so until after all profiles are destroyed.
  virtual UkmDatabase* GetUkmDatabase() = 0;

  // Called to check if Ukm database exist. Ukm database might be null if
  // initialized within tests.
  virtual bool HasUkmDatabase() = 0;

  // Called when a new UKM entry is added.
  virtual void OnEntryAdded(ukm::mojom::UkmEntryPtr entry) = 0;

  // Called when UKM source URL is updated.
  virtual void OnUkmSourceUpdated(ukm::SourceId source_id,
                                  const std::vector<GURL>& urls) = 0;

  // Keep track of all the segmentation services that hold reference to this
  // object.
  virtual void AddRef() = 0;
  virtual void RemoveRef() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_
