// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_shared_remotes.h"

namespace nearby {

namespace {

// This object is owned by SharingImpl and made available for
// ImplementationPlatform which implements a completely static interface.
NearbySharedRemotes* g_instance = nullptr;

}  // namespace

// static
NearbySharedRemotes* NearbySharedRemotes::GetInstance() {
  return g_instance;
}

// static
void NearbySharedRemotes::SetInstance(NearbySharedRemotes* instance) {
  g_instance = instance;
}

NearbySharedRemotes::NearbySharedRemotes() = default;
NearbySharedRemotes::~NearbySharedRemotes() = default;

}  // namespace nearby
