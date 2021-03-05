// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "chrome/updater/mac/update_service_internal_proxy.h"
#include "chrome/updater/mac/update_service_proxy.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

scoped_refptr<UpdateService> CreateUpdateService() {
  return base::MakeRefCounted<UpdateServiceProxy>(GetProcessScope());
}

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternal() {
  return base::MakeRefCounted<UpdateServiceInternalProxy>(GetProcessScope());
}

}  // namespace updater
