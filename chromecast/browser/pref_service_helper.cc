// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/pref_service_helper.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/chromecast_buildflags.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/pref_store.h"

#if defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)
#include "components/cdm/browser/media_drm_storage_impl.h"
#endif  // defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)

namespace chromecast {
namespace shell {

namespace {

void UserPrefsLoadError(PersistentPrefStore::PrefReadError* error_val,
                        PersistentPrefStore::PrefReadError error) {
  DCHECK(error_val);
  *error_val = error;
}

base::FilePath GetConfigPath() {
  base::FilePath config_path;
  CHECK(base::PathService::Get(FILE_CAST_CONFIG, &config_path));
  return config_path;
}

}  // namespace

// static
std::unique_ptr<PrefService> PrefServiceHelper::CreatePrefService(
    PrefRegistrySimple* registry) {
  const base::FilePath config_path(GetConfigPath());
  DVLOG(1) << "Loading config from " << config_path.value();

  registry->RegisterBooleanPref(prefs::kMetricsIsNewClientID, false);
  // Opt-in stats default to true to handle two different cases:
  //  1) Any crashes or UMA logs are recorded prior to setup completing
  //     successfully (even though we can't send them yet).  Unless the user
  //     ends up actually opting out, we don't want to lose this data once
  //     we get network connectivity and are able to send it.  If the user
  //     opts out, nothing further will be sent (honoring the user's setting).
  //  2) Dogfood users (see dogfood agreement).
  registry->RegisterBooleanPref(prefs::kOptInStats, true);
  registry->RegisterListPref(prefs::kActiveDCSExperiments);
  registry->RegisterDictionaryPref(prefs::kLatestDCSFeatures);

#if defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
#endif  // defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)

  RegisterPlatformPrefs(registry);

  PrefServiceFactory prefServiceFactory;
  prefServiceFactory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(config_path));
  prefServiceFactory.set_async(false);

  PersistentPrefStore::PrefReadError prefs_read_error =
      PersistentPrefStore::PREF_READ_ERROR_NONE;
  prefServiceFactory.set_read_error_callback(
      base::Bind(&UserPrefsLoadError, &prefs_read_error));

  std::unique_ptr<PrefService> pref_service(
      prefServiceFactory.Create(registry));
  if (prefs_read_error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "Cannot initialize chromecast config: "
               << config_path.value()
               << ", pref_error=" << prefs_read_error;
  }

  OnPrefsLoaded(pref_service.get());
  return pref_service;
}

}  // namespace shell
}  // namespace chromecast
