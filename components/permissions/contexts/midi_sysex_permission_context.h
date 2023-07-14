// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class HostContentSettingsMap;

namespace content {
class BrowserContext;
}  // namespace content

namespace permissions {

class MidiSysexPermissionContext : public PermissionContextBase {
 public:
  explicit MidiSysexPermissionContext(content::BrowserContext* browser_context);
  MidiSysexPermissionContext(const MidiSysexPermissionContext&) = delete;
  MidiSysexPermissionContext& operator=(const MidiSysexPermissionContext&) =
      delete;
  ~MidiSysexPermissionContext() override;

 private:
  // content_settings::Observer
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // PermissionContextBase:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_
