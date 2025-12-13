// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace permissions {

class MidiSysexPermissionContext : public ContentSettingPermissionContextBase {
 public:
  explicit MidiSysexPermissionContext(content::BrowserContext* browser_context);
  MidiSysexPermissionContext(const MidiSysexPermissionContext&) = delete;
  MidiSysexPermissionContext& operator=(const MidiSysexPermissionContext&) =
      delete;
  ~MidiSysexPermissionContext() override;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const PermissionRequestData& request_data,
                        bool allowed) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_
