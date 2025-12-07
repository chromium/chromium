// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_permission_context.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace permissions {

MidiPermissionContext::MidiPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::MIDI,
          network::mojom::PermissionsPolicyFeature::kMidiFeature) {}

MidiPermissionContext::~MidiPermissionContext() = default;

ContentSetting MidiPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
    return PermissionsClient::Get()
        ->GetSettingsMap(browser_context())
        ->GetContentSetting(requesting_origin, embedding_origin,
                            ContentSettingsType::MIDI_SYSEX);
  }
  return CONTENT_SETTING_ALLOW;
}

}  // namespace permissions
