// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/sync_helper.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "components/app_constants/constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/manifest_url_handlers.h"

namespace extensions {
namespace sync_helper {

bool IsSyncable(const Extension* extension) {
  // Default apps are not synced because otherwise they will pollute profiles
  // that don't already have them. Specially, if a user doesn't have default
  // apps, creates a new profile (which get default apps) and then enables sync
  // for it, then their profile everywhere gets the default apps.
  bool is_syncable =
      (extension->location() == mojom::ManifestLocation::kInternal &&
       !extension->was_installed_by_default());
  if (!is_syncable && !IsSyncableComponentExtension(extension)) {
    // We have a non-standard location.
    return false;
  }

  // Disallow extensions with non-gallery auto-update URLs for now.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (!ManifestURL::GetUpdateURL(extension).is_empty() &&
      !ManifestURL::UpdatesFromGallery(extension)) {
    return false;
  }

  switch (extension->GetType()) {
    case Manifest::Type::kExtension:
    case Manifest::Type::kHostedApp:
    case Manifest::Type::kLegacyPackagedApp:
    case Manifest::Type::kPlatformApp:
    case Manifest::Type::kTheme:
      return true;

    case Manifest::Type::kUserScript:
      // We only want to sync user scripts with gallery update URLs.
      if (ManifestURL::UpdatesFromGallery(extension))
        return true;
      return false;

    case Manifest::Type::kUnknown:
    case Manifest::Type::kSharedModule:
    case Manifest::Type::kLoginScreenExtension:
    case Manifest::Type::kChromeOSSystemExtension:
      return false;

    case Manifest::Type::kNumLoadTypes:
      NOTREACHED();
  }
  NOTREACHED();
}

bool IsSyncableComponentExtension(const Extension* extension) {
  if (!Manifest::IsComponentLocation(extension->location()))
    return false;
  return (extension->id() == extensions::kWebStoreAppId) ||
         (extension->id() == app_constants::kChromeAppId);
}

}  // namespace sync_helper
}  // namespace extensions
