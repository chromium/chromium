// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper to initialize PrefService for cast shell.

#ifndef CHROMECAST_BROWSER_PREF_SERVICE_HELPER_H_
#define CHROMECAST_BROWSER_PREF_SERVICE_HELPER_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "chromecast/base/process_types.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;

namespace chromecast {
namespace shell {

// It uses JsonPrefStore internally and/so the format of config file is same to
// that of JsonPrefStore.
class PrefServiceHelper {
 public:
  // Loads configs from config file. Returns true if successful.
  static std::unique_ptr<PrefService> CreatePrefService(
      PrefRegistrySimple* registry,
      ProcessType process_type = ProcessType::kCastService);

  // Provides names of prefs that take a large amount of storage, and are
  // therefore stored in a different file.
  static PrefNameSet LargePrefNames() __attribute__((weak));

 private:
  // Registers any needed preferences for the current platform.
  static void RegisterPlatformPrefs(PrefRegistrySimple* registry);

  // Called after the pref file has been loaded.
  static void OnPrefsLoaded(PrefService* pref_service);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_PREF_SERVICE_HELPER_H_
