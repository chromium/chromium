// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

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
  // PermissionContextBase:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_MIDI_SYSEX_PERMISSION_CONTEXT_H_
