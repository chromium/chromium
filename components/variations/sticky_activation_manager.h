// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_STICKY_ACTIVATION_MANAGER_H_
#define COMPONENTS_VARIATIONS_STICKY_ACTIVATION_MANAGER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

// Manages the set of field trials marked with the activation type
// STICKY_AFTER_QUERY. Responsible for persisting information about activated
// trials to Local State and using that information to determine if a trial
// should be activated on the next startup.
//
// A sticky trial should be activated on startup if it was active in the
// previous session (including from stickiness on startup) and its group
// selection did not change (i.e. due to a change to its config or something
// external like the client's randomization inputs).
//
// TODO: crbug.com/435630455 - This class is under development.
class COMPONENT_EXPORT(VARIATIONS) StickyActivationManager {
 public:
  // `local_state` may be null for tests, in which case no prior stickiness
  // information will be loaded and none will be saved.
  explicit StickyActivationManager(PrefService* local_state);

  StickyActivationManager(const StickyActivationManager&) = delete;
  StickyActivationManager& operator=(const StickyActivationManager&) = delete;

  ~StickyActivationManager();

  // Registers the prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple& registry);

 private:
  raw_ptr<PrefService> local_state_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_STICKY_ACTIVATION_MANAGER_H_
