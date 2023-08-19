// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_sysex_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/child_process_security_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

namespace permissions {

MidiSysexPermissionContext::MidiSysexPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::MIDI_SYSEX,
          blink::mojom::PermissionsPolicyFeature::kMidiFeature),
      host_content_settings_map_(
          permissions::PermissionsClient::Get()->GetSettingsMap(
              browser_context)) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kBlockMidiByDefault)) {
    content_setting_observer_registered_by_subclass_ = true;
    host_content_settings_map_->AddObserver(this);
  }
}

MidiSysexPermissionContext::~MidiSysexPermissionContext() {
  if (base::FeatureList::IsEnabled(
          permissions::features::kBlockMidiByDefault)) {
    host_content_settings_map_->RemoveObserver(this);
  }
}

void MidiSysexPermissionContext::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  PermissionContextBase::OnContentSettingChanged(
      primary_pattern, secondary_pattern, content_type_set);

  // Synchronize the base MIDI permission
  if (base::FeatureList::IsEnabled(
          permissions::features::kBlockMidiByDefault)) {
    if (content_type_set.Contains(ContentSettingsType::MIDI_SYSEX)) {
      // TODO(crbug.com/1078272): We should not need to deduce the url from
      // the primary pattern here. Modify the infrastructure to facilitate
      // this particular use case better.
      const GURL url(primary_pattern.ToString());
      if (!url::Origin::Create(url).opaque()) {
        const ContentSetting midi_setting =
            host_content_settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MIDI);
        const ContentSetting midi_sysex_setting =
            host_content_settings_map_->GetContentSetting(
                url, url, ContentSettingsType::MIDI_SYSEX);

        switch (midi_sysex_setting) {
          case CONTENT_SETTING_BLOCK:
            if (midi_setting != CONTENT_SETTING_BLOCK) {
              host_content_settings_map_->SetContentSettingCustomScope(
                  primary_pattern, secondary_pattern, ContentSettingsType::MIDI,
                  CONTENT_SETTING_BLOCK);
            }
            break;
          case CONTENT_SETTING_ASK:
            if (midi_setting != CONTENT_SETTING_ASK &&
                midi_setting != CONTENT_SETTING_ALLOW) {
              host_content_settings_map_->SetContentSettingCustomScope(
                  primary_pattern, secondary_pattern, ContentSettingsType::MIDI,
                  CONTENT_SETTING_ASK);
            }
            break;
          case CONTENT_SETTING_ALLOW:
            if (midi_setting != CONTENT_SETTING_ALLOW) {
              host_content_settings_map_->SetContentSettingCustomScope(
                  primary_pattern, secondary_pattern, ContentSettingsType::MIDI,
                  CONTENT_SETTING_ALLOW);
            }
            break;
          default:
            break;
        }
      }
    }
  }
}

void MidiSysexPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                                  const GURL& requesting_frame,
                                                  bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnContentAllowed(ContentSettingsType::MIDI_SYSEX);

    content::ChildProcessSecurityPolicy::GetInstance()
        ->GrantSendMidiSysExMessage(id.global_render_frame_host_id().child_id);
  } else {
    content_settings->OnContentBlocked(ContentSettingsType::MIDI_SYSEX);
  }
}

}  // namespace permissions
