// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_PREFS_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_PREFS_H_

#include "base/component_export.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

// Register prefs required for legacy pref-based PINs.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
void RegisterPinStoragePrefs(PrefRegistrySimple* registry);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_PREFS_H_
