// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_CONSTANTS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_CONSTANTS_H_

#include <stddef.h>

namespace storage {

// The quota for each storage area.
// This value is enforced by clients and by the storage service.
extern const size_t kPerStorageAreaQuota;

// In the storage service we allow some overage to
// accommodate concurrent writes from different clients
// that were allowed because the limit imposed in the client
// wasn't exceeded.
extern const size_t kPerStorageAreaOverQuotaAllowance;

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_CONSTANTS_H_
