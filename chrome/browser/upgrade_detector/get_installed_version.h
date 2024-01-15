// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_GET_INSTALLED_VERSION_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_GET_INSTALLED_VERSION_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/version.h"

struct InstalledAndCriticalVersion {
  explicit InstalledAndCriticalVersion(base::Version the_installed_version);
  InstalledAndCriticalVersion(base::Version the_installed_version,
                              base::Version the_critical_version);
  InstalledAndCriticalVersion(InstalledAndCriticalVersion&& other) noexcept;
  InstalledAndCriticalVersion& operator=(InstalledAndCriticalVersion&& other) =
      default;
  ~InstalledAndCriticalVersion();

  // The installed version (not necessarily the one that is running). May be
  // invalid in case of failure reading the installed version.
  base::Version installed_version;

  // An optional critical version, indicating a minimum version that must be
  // running. A running version lower than this is presumed to have a critical
  // flaw sufficiently important that it must be updated as soon as possible.
  std::optional<base::Version> critical_version;
};

// A platform-specific function that invokes a callback with the currently
// installed version and an optional critical version.
using InstalledVersionCallback =
    base::OnceCallback<void(InstalledAndCriticalVersion)>;

// Triggers the callback with the currently installed version and an optional
// critical version (Windows only as of this writing). This function may block
// the thread on which it runs.
void GetInstalledVersion(InstalledVersionCallback callback);

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_GET_INSTALLED_VERSION_H_
