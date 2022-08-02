// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolation_prefs_utils.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "url/origin.h"

namespace web_app {

// The stored preferences managed by this file look like:
// "web_apps": {
//   ... other fields managed by web_app_prefs_utils ...
//
//   "isolation_state": {
//     "<origin>": {
//       "storage_isolation_key": "abc123",
//     },
//   },
// }

namespace {

const char kStorageIsolationKey[] = "storage_isolation_key";

// Creates a copy of the given origin but without a port set. This is a
// temporary hack meant to work around the fact that we key app isolation state
// on the app's origin, but StoragePartitions are looked up based on sites.
// Removing the port does not convert an origin into a site, but the actual
// origin to site logic is private to //content and this is good enough to
// allow testing in the short term.
// TODO(crbug.com/1212263): Remove this function.
url::Origin RemovePort(const url::Origin& origin) {
  return url::Origin::CreateFromNormalizedTuple(origin.scheme(), origin.host(),
                                                /*port=*/0);
}

}  // namespace

void IsolationPrefsUtilsRegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(::prefs::kWebAppsIsolationState);
}

void RecordOrRemoveAppIsolationState(PrefService* pref_service,
                                     const WebApp& web_app) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url::Origin origin = RemovePort(url::Origin::Create(web_app.scope()));

  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsIsolationState);
  if (web_app.IsStorageIsolated()) {
    std::unique_ptr<prefs::DictionaryValueUpdate> origin_isolation_update;
    if (!update->GetDictionaryWithoutPathExpansion(origin.Serialize(),
                                                   &origin_isolation_update)) {
      origin_isolation_update = update->SetDictionaryWithoutPathExpansion(
          origin.Serialize(), std::make_unique<base::DictionaryValue>());
    }
    origin_isolation_update->SetString(kStorageIsolationKey, web_app.app_id());
  } else {
    update->RemoveWithoutPathExpansion(origin.Serialize(), nullptr);
  }
}

void RemoveAppIsolationState(PrefService* pref_service,
                             const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsIsolationState);
  update->RemoveWithoutPathExpansion(RemovePort(origin).Serialize(), nullptr);
}

const std::string* GetStorageIsolationKey(PrefService* pref_service,
                                          const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& isolation_prefs =
      pref_service->GetValueDict(prefs::kWebAppsIsolationState);

  const base::Value::Dict* origin_prefs =
      isolation_prefs.FindDict(RemovePort(origin).Serialize());
  if (!origin_prefs)
    return nullptr;
  return origin_prefs->FindString(kStorageIsolationKey);
}

}  // namespace web_app
