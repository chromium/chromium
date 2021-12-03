// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/service_proxy_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// TODO(crbug.com/1276169) - implement.
scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope /*updater_scope*/) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace updater
