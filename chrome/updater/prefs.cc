// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/util.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"

namespace updater {

std::unique_ptr<PrefService> CreatePrefService() {
  base::FilePath product_data_dir;
  if (!GetProductDirectory(&product_data_dir))
    return nullptr;

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      product_data_dir.Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());

  return pref_service_factory.Create(pref_registry);
}

}  // namespace updater
