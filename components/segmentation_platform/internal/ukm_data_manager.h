// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_

namespace base {
class FilePath;
}

namespace ukm {
class UkmRecorderImpl;
}

namespace segmentation_platform {

class UkmDatabase;
class UrlSignalHandler;
class UkmConfig;

// Manages ownership and lifetime of all UKM related classes, like database and
// observer. There is only one manager per browser process. Created before
// profile initialization and destroyed after all profiles are destroyed. The
// database, observer and signal handler can have different lifetimes, see
// comments below.
class UkmDataManager {
 public:
  UkmDataManager() = default;
  virtual ~UkmDataManager() = default;

  UkmDataManager(UkmDataManager&) = delete;
  UkmDataManager& operator=(UkmDataManager&) = delete;

  // Initializes UKM database.
  virtual void Initialize(const base::FilePath& database_path) = 0;

  // Returns true when UKM engine is usable. If false, then UKM based engine is
  // disabled and this class is a no-op. UkmObserver, UrlSignalHandler and
  // UkmDatabase are not created and are unusable when this method returns
  // false.
  virtual bool IsUkmEngineEnabled() = 0;

  // Must be called when UKM service is available to start observing metrics.
  virtual void NotifyCanObserveUkm(ukm::UkmRecorderImpl* ukm_recorder) = 0;

  // Can be called at any time, irrespective of UKM observer's lifetime. If
  // NotifyCanObserveUkm() was already called, then starts observing UKM with
  // the given config. Else, starts when NotifyCanObserveUkm() is called. If
  // called after StopObservingUkm(), does nothing.
  virtual void StartObservingUkm(const UkmConfig& config) = 0;

  // Pauses or resumes observation of UKM, can be called any time, irrespective
  // of UKM observer's lifetime, similar to StartObservingUkm().
  virtual void PauseOrResumeObservation(bool pause) = 0;

  // Must be called before UKM service is destroyed, to remove observers.
  virtual void StopObservingUkm() = 0;

  // Get URL signal handler. The signal handler is safe to use as long as data
  // manager is alive, so until after all profiles are destroyed.
  virtual UrlSignalHandler* GetOrCreateUrlHandler() = 0;

  // Get UKM database. The database is safe to use as long as data manager is
  // alive, so until after all profiles are destroyed.
  virtual UkmDatabase* GetUkmDatabase() = 0;

  // Keep track of all the segmentation services that hold reference to this
  // object.
  virtual void AddRef() = 0;
  virtual void RemoveRef() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_
