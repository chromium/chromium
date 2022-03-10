// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_api_permission_context.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"

FederatedIdentityApiPermissionContext::FederatedIdentityApiPermissionContext(
    content::BrowserContext* browser_context)
    : host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(browser_context)),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context))) {}

FederatedIdentityApiPermissionContext::
    ~FederatedIdentityApiPermissionContext() = default;

bool FederatedIdentityApiPermissionContext::HasApiPermission() {
  return host_content_settings_map_->GetDefaultContentSetting(
             ContentSettingsType::FEDERATED_IDENTITY_API, nullptr) !=
         ContentSetting::CONTENT_SETTING_BLOCK;
}

bool FederatedIdentityApiPermissionContext::AreThirdPartyCookiesBlocked() {
  return cookie_settings_->ShouldBlockThirdPartyCookies();
}
