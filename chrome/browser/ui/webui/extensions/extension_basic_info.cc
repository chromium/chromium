// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"

namespace {

// Keys in the dictionary returned by GetExtensionBasicInfo().
const char kDescriptionKey[] = "description";
const char kDetailsUrlKey[] = "detailsUrl";
const char kEnabledKey[] = "enabled";
const char kHomepageUrlKey[] = "homepageUrl";
const char kInfoIdKey[] = "id";
const char kInfoNameKey[] = "name";
const char kInfoVersionKey[] = "version";
const char kKioskEnabledKey[] = "kioskEnabled";
const char kKioskOnlyKey[] = "kioskOnly";
const char kOfflineEnabledKey[] = "offlineEnabled";
const char kOptionsUrlKey[] = "optionsUrl";
const char kPackagedAppKey[] = "packagedApp";

}  // namespace

namespace extensions {

void GetExtensionBasicInfo(const Extension* extension,
                           bool enabled,
                           base::Value::Dict* info) {
  info->Set(kInfoIdKey, extension->id());
  info->Set(kInfoNameKey, extension->name());
  info->Set(kEnabledKey, enabled);
  info->Set(kKioskEnabledKey, KioskModeInfo::IsKioskEnabled(extension));
  info->Set(kKioskOnlyKey, KioskModeInfo::IsKioskOnly(extension));
  info->Set(kOfflineEnabledKey,
            OfflineEnabledInfo::IsOfflineEnabled(extension));
  info->Set(kInfoVersionKey, extension->GetVersionForDisplay());
  info->Set(kDescriptionKey, extension->description());
  info->Set(kOptionsUrlKey,
            OptionsPageInfo::GetOptionsPage(extension).possibly_invalid_spec());
  info->Set(kHomepageUrlKey,
            ManifestURL::GetHomepageURL(extension).possibly_invalid_spec());
  info->Set(kDetailsUrlKey,
            ManifestURL::GetDetailsURL(extension).possibly_invalid_spec());
  info->Set(kPackagedAppKey, extension->is_platform_app());
}

}  // namespace extensions
