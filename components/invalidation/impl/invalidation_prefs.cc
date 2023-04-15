// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_prefs.h"

namespace invalidation::prefs {

// The preference for storing client ID for the invalidator, keyed by sender ID.
const char kInvalidationClientIDCache[] =
    "invalidation.per_sender_client_id_cache";

}  // namespace invalidation::prefs
