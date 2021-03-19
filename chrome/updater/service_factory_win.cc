// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/update_service_internal_proxy.h"
#include "chrome/updater/win/update_service_proxy.h"
#include "chrome/updater/win/wrl_module.h"

namespace updater {
namespace {

// Allows one time creation of the WRL::Module instance. The WRL library
// contains a global instance of a class, which must be created only once.
class WRLModuleInitializer {
 public:
  WRLModuleInitializer() {
    Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
  }

  static const WRLModuleInitializer& Get() {
    static const base::NoDestructor<WRLModuleInitializer> module;
    return *module;
  }
};

}  // namespace

scoped_refptr<UpdateService> CreateUpdateService() {
  WRLModuleInitializer::Get();
  return base::MakeRefCounted<UpdateServiceProxy>(GetProcessScope());
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternal() {
  WRLModuleInitializer::Get();
  return base::MakeRefCounted<UpdateServiceInternalProxy>(GetProcessScope());
}

}  // namespace updater
