// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_UPGRADE_PARAMS_H_
#define COMPONENTS_ARC_SESSION_ARC_UPGRADE_PARAMS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/arc/session/arc_supervision_transition.h"

namespace arc {

constexpr char kPackagesCacheModeCopy[] = "copy";
constexpr char kPackagesCacheModeSkipCopy[] = "skip-copy";

// Parameters to upgrade request.
struct UpgradeParams {
  enum class PackageCacheMode {
    // Performs packages cache setup if the pre-generated cache exists.
    DEFAULT = 0,
    // Performs packages cache setup if the pre-generated cache exists and
    // copies resulting packages.xml to the temporary location after
    // SystemServer initialized the package manager.
    COPY_ON_INIT,
    // Skips packages cache setup and copies resulting packages.xml to the
    // temporary location after SystemServer initialized the package manager.
    SKIP_SETUP_COPY_ON_INIT,
  };

  // Explicit ctor/dtor declaration is necessary for complex struct. See
  // https://cs.chromium.org/chromium/src/tools/clang/plugins/FindBadConstructsConsumer.cpp
  UpgradeParams();
  ~UpgradeParams();
  // Intentionally allows copying. The parameter is for container restart.
  UpgradeParams(const UpgradeParams& other);
  UpgradeParams(UpgradeParams&& other);
  UpgradeParams& operator=(UpgradeParams&& other);

  // Account ID of the user to start ARC for.
  std::string account_id;

  // Whether the account is managed.
  bool is_account_managed;

  // Option to disable ACTION_BOOT_COMPLETED broadcast for 3rd party apps.
  // The constructor automatically populates this from command-line.
  bool skip_boot_completed_broadcast;

  // Optional mode for packages cache tests.
  // The constructor automatically populates this from command-line.
  PackageCacheMode packages_cache_mode;

  // Option to disable GMS CORE cache.
  // The constructor automatically populates this from command-line.
  bool skip_gms_core_cache;

  // The supervision transition state for this account. Indicates whether
  // child account should become regular, regular account should become child
  // or neither.
  ArcSupervisionTransition supervision_transition =
      ArcSupervisionTransition::NO_TRANSITION;

  // Define language configuration set during Android container boot.
  // |preferred_languages| may be empty.
  std::string locale;
  std::vector<std::string> preferred_languages;

  // Whether ARC is being upgraded in a demo session.
  bool is_demo_session = false;

  // |demo_session_apps_path| is a file path to the image containing set of
  // demo apps that should be pre-installed into the Android container for
  // demo sessions. It might be empty, in which case no demo apps will be
  // pre-installed.
  // Should be empty if |is_demo_session| is not set.
  base::FilePath demo_session_apps_path;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_UPGRADE_PARAMS_H_
