// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

namespace permissions {

MidiPermissionContext::MidiPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::MIDI,
          blink::mojom::PermissionsPolicyFeature::kMidiFeature) {}

MidiPermissionContext::~MidiPermissionContext() = default;

ContentSetting MidiPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (base::FeatureList::IsEnabled(features::kBlockMidiByDefault)) {
    return PermissionContextBase::GetPermissionStatusInternal(
        render_frame_host, requesting_origin, embedding_origin);
  }
  return CONTENT_SETTING_ALLOW;
}

void MidiPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                             const GURL& requesting_frame,
                                             bool allowed) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kBlockMidiByDefault)) {
    content_settings::PageSpecificContentSettings* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            id.global_render_frame_host_id());
    if (!content_settings) {
      return;
    }

    if (allowed) {
      content_settings->OnContentAllowed(ContentSettingsType::MIDI);

      content::ChildProcessSecurityPolicy::GetInstance()->GrantSendMidiMessage(
          id.global_render_frame_host_id().child_id);
    } else {
      content_settings->OnContentBlocked(ContentSettingsType::MIDI);
    }
  }
}

}  // namespace permissions
