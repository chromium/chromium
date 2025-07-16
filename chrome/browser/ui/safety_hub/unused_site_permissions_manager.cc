// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kUnknownContentSettingsType[] = "unknown";

}  // namespace

// static
std::string UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
    ContentSettingsType type) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  DCHECK(website_setting_registry);

  auto* website_settings_info = website_setting_registry->Get(type);
  if (!website_settings_info) {
    auto integer_type = static_cast<int32_t>(type);
    DVLOG(1) << "Couldn't retrieve website settings info entry from the "
                "registry for type: "
             << integer_type;
    base::UmaHistogramSparse(
        "Settings.SafetyCheck.UnusedSitePermissionsMigrationFail",
        integer_type);
    return kUnknownContentSettingsType;
  }

  return website_settings_info->name();
}

// static
ContentSettingsType
UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
    const std::string& key) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  return website_setting_registry->GetByName(key)->type();
}

// static
url::Origin UnusedSitePermissionsManager::ConvertPrimaryPatternToOrigin(
    const ContentSettingsPattern& primary_pattern) {
  GURL origin_url = GURL(primary_pattern.ToString());
  CHECK(origin_url.is_valid());

  return url::Origin::Create(origin_url);
}

UnusedSitePermissionsManager::UnusedSitePermissionsManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser_context_);
}

UnusedSitePermissionsManager::~UnusedSitePermissionsManager() = default;
