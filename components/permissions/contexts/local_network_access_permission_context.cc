// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/local_network_access_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

LocalNetworkAccessPermissionContext::LocalNetworkAccessPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::LOCAL_NETWORK_ACCESS,
          network::mojom::PermissionsPolicyFeature::kLocalNetworkAccess) {}

LocalNetworkAccessPermissionContext::~LocalNetworkAccessPermissionContext() =
    default;

// Permission is needed for non-secure contexts to allow for the
// LocalNetworkAccessNonSecureContextAllowed reverse origin trial to be enabled;
// otherwise even with the origin trial enabled the perimission is denied.
//
// This should be removed when the LocalNetworkAccessNonSecureContextAllowed
// deprecation trial is removed.
//
// Of note, the registration of ContentSettingsType::LOCAL_NETWORK_ACCESS in
// components/content_settings/core/browser/content_settings_registry.cc
// specifies ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY. This means
// that while a user can enable the permission on sites enrolled in the reverse
// origin trial, they can not enter http exceptions in directly on the settings
// page.
//
// Also of note: the IsRestrictedToSecureOrigins override is not needed if a
// user is bypassing the secure context restriction through the
// --unsafely-treat-insecure-origin-as-secure command line flag (or through the
// OverrideSecurityRestrictionsOnInsecureOrigin enterprise policy).
bool LocalNetworkAccessPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}

}  // namespace permissions
