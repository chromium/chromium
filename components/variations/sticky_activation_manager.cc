// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/sticky_activation_manager.h"

#include "components/prefs/pref_registry_simple.h"

namespace variations {

StickyActivationManager::StickyActivationManager(PrefService* local_state)
    : local_state_(local_state) {}

StickyActivationManager::~StickyActivationManager() = default;

// static
void StickyActivationManager::RegisterPrefs(PrefRegistrySimple& registry) {}

}  // namespace variations
