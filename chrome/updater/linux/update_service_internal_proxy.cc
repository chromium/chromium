// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/service_proxy_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// TODO(crbug.com/1276169) - implement.
scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope /*updater_scope*/) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace updater
