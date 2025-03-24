// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/permission_test_util.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/contexts/window_management_permission_context.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {
namespace {

class FakePermissionContext : public PermissionContextBase {
 public:
  FakePermissionContext(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      network::mojom::PermissionsPolicyFeature permissions_policy_feature)
      : PermissionContextBase(browser_context,
                              content_settings_type,
                              permissions_policy_feature) {}
};

class FakePermissionContextAlwaysAllow : public FakePermissionContext {
 public:
  FakePermissionContextAlwaysAllow(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      network::mojom::PermissionsPolicyFeature permissions_policy_feature)
      : FakePermissionContext(browser_context,
                              content_settings_type,
                              permissions_policy_feature) {}

  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override {
    return CONTENT_SETTING_ALLOW;
  }
};

PermissionManager::PermissionContextMap CreatePermissionContexts(
    content::BrowserContext* browser_context) {
  PermissionManager::PermissionContextMap permission_contexts;
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::GEOLOCATION,
          network::mojom::PermissionsPolicyFeature::kGeolocation);
  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::NOTIFICATIONS,
          network::mojom::PermissionsPolicyFeature::kNotFound);
  permission_contexts[ContentSettingsType::MIDI_SYSEX] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::MIDI_SYSEX,
          network::mojom::PermissionsPolicyFeature::kMidiFeature);
  permission_contexts[ContentSettingsType::MIDI] =
      std::make_unique<FakePermissionContextAlwaysAllow>(
          browser_context, ContentSettingsType::MIDI,
          network::mojom::PermissionsPolicyFeature::kMidiFeature);
  permission_contexts[ContentSettingsType::STORAGE_ACCESS] =
      std::make_unique<FakePermissionContextAlwaysAllow>(
          browser_context, ContentSettingsType::STORAGE_ACCESS,
          network::mojom::PermissionsPolicyFeature::kStorageAccessAPI);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
          network::mojom::PermissionsPolicyFeature::kEncryptedMedia);
#endif
  permission_contexts[ContentSettingsType::WEB_APP_INSTALLATION] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::WEB_APP_INSTALLATION,
          network::mojom::PermissionsPolicyFeature::kWebAppInstallation);
  permission_contexts[ContentSettingsType::WINDOW_MANAGEMENT] =
      std::make_unique<WindowManagementPermissionContext>(browser_context);
  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::MEDIASTREAM_CAMERA,
          network::mojom::PermissionsPolicyFeature::kCamera);
  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::MEDIASTREAM_MIC,
          network::mojom::PermissionsPolicyFeature::kMicrophone);
  permission_contexts[ContentSettingsType::AUTOMATIC_FULLSCREEN] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::AUTOMATIC_FULLSCREEN,
          network::mojom::PermissionsPolicyFeature::kFullscreen);
  permission_contexts[ContentSettingsType::LOCAL_NETWORK_ACCESS] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::LOCAL_NETWORK_ACCESS,
          network::mojom::PermissionsPolicyFeature::kNotFound);
  return permission_contexts;
}

}  // namespace

std::unique_ptr<content::PermissionControllerDelegate>
GetPermissionControllerDelegate(content::BrowserContext* context) {
  return std::make_unique<permissions::PermissionManager>(
      context, CreatePermissionContexts(context));
}

}  // namespace permissions
