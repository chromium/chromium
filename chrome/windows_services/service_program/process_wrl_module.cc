// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/process_wrl_module.h"

#include <wrl/module.h>

#include <utility>

#include "base/no_destructor.h"

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
    callback_ = std::move(callback);
  }

  // A method invoked by the WRL::Module's release notifier. Runs the held
  // callback, if any.
  void OnModuleReleased() {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  friend class base::NoDestructor<ModuleReleaseHelper>;

  ModuleReleaseHelper() = default;

  base::OnceClosure callback_;
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
