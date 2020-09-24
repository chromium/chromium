// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "chrome/updater/service_scope.h"
#include "chrome/updater/win/control_service_out_of_process.h"
#include "chrome/updater/win/update_service_out_of_process.h"

namespace updater {

scoped_refptr<UpdateService> CreateUpdateService() {
  return base::MakeRefCounted<UpdateServiceOutOfProcess>(GetProcessScope());
}

scoped_refptr<ControlService> CreateControlService() {
  return base::MakeRefCounted<ControlServiceOutOfProcess>(GetProcessScope());
}

}  // namespace updater
