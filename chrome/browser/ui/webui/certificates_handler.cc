// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificates_handler.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace certificate_manager {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Allow users to manage all client certificates by default. This can be
  // overridden by enterprise policy.
  registry->RegisterIntegerPref(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kAll));
}

}  // namespace certificate_manager
