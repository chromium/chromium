// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOREALIS_BOREALIS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_BOREALIS_BOREALIS_UTIL_H_

#include <string>

namespace aura {
class Window;
}

namespace ash::borealis {

// Borealis windows are created with app/startup ids beginning with this.
extern const char kBorealisWindowPrefix[];

// Anonymous apps do not have a CrOS-standard app_id (i.e. one registered with
// the GuestOsRegistryService), so to identify them we prepend this.
extern const char kBorealisAnonymousPrefix[];

bool IsBorealisWindowId(const std::string& wayland_window_id);

bool IsBorealisWindow(const aura::Window* window);

}  // namespace ash::borealis

#endif  // CHROMEOS_ASH_COMPONENTS_BOREALIS_BOREALIS_UTIL_H_
