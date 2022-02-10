// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_

#include "base/files/file_path.h"

namespace ukm {
class UkmRecorderImpl;
}

namespace segmentation_platform {

class UkmDatabase;
class UrlSignalHandler;

// Manages ownership and lifetime of all UKM related classes, like database and
// observer. There is only one manager per browser process. Created before
// profile initialization and destroyed after all profiles are destroyed. The
// database, observer and signal handler can have different lifetimes, see
// comments below.
class UkmDataManager {
 public:
  UkmDataManager();
  ~UkmDataManager();

  UkmDataManager(UkmDataManager&) = delete;
  UkmDataManager& operator=(UkmDataManager&) = delete;

  // Initializes UKM database.
  void Initialize(const base::FilePath& database_path);

  // Must be called when UKM service is available to start observing metrics.
  void CanObserveUkm(ukm::UkmRecorderImpl* ukm_recorder);
  // Must be called before UKM service is destroyed, to remove observers.
  void StopObservingUkm();

  // Get URL signal handler. The signal handler is safe to use as long as data
  // manager is alive, so until after all profiles are destroyed.
  UrlSignalHandler* GetOrCreateUrlHandler();

  // Get UKM database. The database is safe to use as long as data manager is
  // alive, so until after all profiles are destroyed.
  UkmDatabase* GetUkmDatabase();

  // Keep track of all the segmentation services that hold reference to this
  // object.
  void AddRef();
  void RemoveRef();

 private:
  int ref_count_{0};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_UKM_DATA_MANAGER_H_
