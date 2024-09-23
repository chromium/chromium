// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_sysex_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/child_process_security_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

namespace permissions {

MidiSysexPermissionContext::MidiSysexPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::MIDI_SYSEX,
          blink::mojom::PermissionsPolicyFeature::kMidiFeature) {}

MidiSysexPermissionContext::~MidiSysexPermissionContext() = default;

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
