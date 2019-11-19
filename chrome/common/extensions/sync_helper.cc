// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/sync_helper.h"

#include "base/logging.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {
namespace sync_helper {

bool IsSyncable(const Extension* extension) {
  const Feature* feature =
      FeatureProvider::GetBehaviorFeature(behavior_feature::kDoNotSync);
  if (feature && feature->IsAvailableToExtension(extension).is_available())
    return false;

  // Default apps are not synced because otherwise they will pollute profiles
  // that don't already have them. Specially, if a user doesn't have default
  // apps, creates a new profile (which get default apps) and then enables sync
  // for it, then their profile everywhere gets the default apps.
  bool is_syncable = (extension->location() == Manifest::INTERNAL &&
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
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
    case Manifest::TYPE_THEME:
      return true;

    case Manifest::TYPE_USER_SCRIPT:
      // We only want to sync user scripts with gallery update URLs.
      if (ManifestURL::UpdatesFromGallery(extension))
        return true;
      return false;

    case Manifest::TYPE_UNKNOWN:
    case Manifest::TYPE_SHARED_MODULE:
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return false;

    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }
  NOTREACHED();
  return false;
}

bool IsSyncableComponentExtension(const Extension* extension) {
  if (!Manifest::IsComponentLocation(extension->location()))
    return false;
  return (extension->id() == extensions::kWebStoreAppId) ||
         (extension->id() == extension_misc::kChromeAppId);
}

}  // namespace sync_helper
}  // namespace extensions
