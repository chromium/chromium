// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MIGRATION_MIGRATION_UTILS_H_
#define CHROMECAST_BROWSER_MIGRATION_MIGRATION_UTILS_H_

namespace chromecast {
namespace cast_browser_migration {

// Copies the pref config files (one regular config file and one dedicated large
// config file for DCS features). Returns true if each config file is
// successfully copied or already exists.
bool CopyPrefConfigsIfMissing();

}  // namespace cast_browser_migration
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MIGRATION_MIGRATION_UTILS_H_
