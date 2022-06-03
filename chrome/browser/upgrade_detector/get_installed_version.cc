// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

InstalledAndCriticalVersion::InstalledAndCriticalVersion(
    base::Version the_installed_version)
    : installed_version(std::move(the_installed_version)) {}

InstalledAndCriticalVersion::InstalledAndCriticalVersion(
    base::Version the_installed_version,
    base::Version the_critical_version)
    : installed_version(std::move(the_installed_version)),
      critical_version(std::move(the_critical_version)) {}

InstalledAndCriticalVersion::InstalledAndCriticalVersion(
    InstalledAndCriticalVersion&& other) noexcept
    : installed_version(std::move(other.installed_version)),
      critical_version(std::move(other.critical_version)) {}

InstalledAndCriticalVersion::~InstalledAndCriticalVersion() = default;
