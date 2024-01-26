// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SERVICE_CAST_SERVICE_H_
#define CHROMECAST_SERVICE_CAST_SERVICE_H_

#include <memory>

namespace base {
class ThreadChecker;
}

namespace chromecast {

class CastService {
 public:
  CastService(const CastService&) = delete;
  CastService& operator=(const CastService&) = delete;

  virtual ~CastService();

  // Initializes/finalizes the cast service.
  void Initialize();
  void Finalize();

  // Starts/stops the cast service.
  void Start();
  void Stop();

 protected:
  CastService();

  // Implementation-specific initialization. Initialization of cast service's
  // sub-components, and anything that requires IO operations should go here.
  // Anything that should happen before cast service is started but doesn't need
  // the sub-components to finish initializing should also go here.
  virtual void InitializeInternal() = 0;

  // Implementation-specific finalization. Any initializations done by
  // InitializeInternal() should be finalized here.
  virtual void FinalizeInternal() = 0;

  // Implementation-specific start behavior. It basically starts the
  // sub-component services and does additional initialization that cannot be
  // done in the InitializationInternal().
  virtual void StartInternal() = 0;

  // Implementation-specific stop behavior. Any initializations done by
  // StartInternal() should be finalized here.
  virtual void StopInternal() = 0;

 private:
  bool stopped_;
  const std::unique_ptr<base::ThreadChecker> thread_checker_;
};

}  // namespace chromecast

#endif  // CHROMECAST_SERVICE_CAST_SERVICE_H_
