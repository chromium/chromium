// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_prefs.h"

namespace invalidation {
namespace prefs {

// The prefference for storing client ID for the invalidator.
const char kFCMInvalidationClientIDCacheDeprecated[] =
    "fcm.invalidation.client_id_cache";

// The preference for storing client ID for the invalidator, keyed by sender ID.
const char kInvalidationClientIDCache[] =
    "invalidation.per_sender_client_id_cache";

}  // namespace prefs
}  // namespace invalidation
