// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_QUALIFYING_H_
#define CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_QUALIFYING_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

class Configurator;
class LocalPrefs;
class UpdateServiceInternal;

// Runs in the thread pool. May block. May use sync primitives. Returns true
// iff the health checks pass.
bool DoPlatformSpecificHealthChecks(UpdaterScope scope);

scoped_refptr<UpdateServiceInternal> MakeQualifyingUpdateServiceInternal(
    scoped_refptr<Configurator> config,
    scoped_refptr<LocalPrefs> local_prefs);

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_INTERNAL_IMPL_QUALIFYING_H_
