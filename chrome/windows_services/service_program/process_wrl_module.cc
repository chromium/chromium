// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/process_wrl_module.h"

#include <wrl/module.h>

#include <utility>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace {

// A process-wide singleton by necessity, as WRL::Module requires a fixed
// object/method pair at construction.
class ModuleReleaseHelper {
 public:
  ModuleReleaseHelper(const ModuleReleaseHelper&) = delete;
  ModuleReleaseHelper& operator=(const ModuleReleaseHelper&) = delete;

  static ModuleReleaseHelper& GetInstance() {
    static base::NoDestructor<ModuleReleaseHelper> instance;
    return *instance;
  }

  // Sets the callback to be run when the last reference to the module is
  // released.
  void SetModuleReleasedCallback(base::OnceClosure callback) {
    base::AutoLock lock(lock_);
    callback_ = std::move(callback);
  }

  // A method invoked by the WRL::Module's release notifier. Runs the held
  // callback, if any.
  void OnModuleReleased() {
    base::OnceClosure callback;
    {
      base::AutoLock lock(lock_);
      callback = std::move(callback_);
    }
    if (callback) {
      std::move(callback).Run();
    }
  }

 private:
  friend class base::NoDestructor<ModuleReleaseHelper>;

  ModuleReleaseHelper() = default;

  base::Lock lock_;
  base::OnceClosure callback_ GUARDED_BY(lock_);
};

}  // namespace

void CreateWrlModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      &ModuleReleaseHelper::GetInstance(),
      &ModuleReleaseHelper::OnModuleReleased);
}

void SetModuleReleasedCallback(base::OnceClosure callback) {
  ModuleReleaseHelper::GetInstance().SetModuleReleasedCallback(
      std::move(callback));
}
