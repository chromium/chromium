// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_KEY_SYSTEM_SUPPORT_UPDATE_H_
#define COMPONENTS_CDM_RENDERER_KEY_SYSTEM_SUPPORT_UPDATE_H_

#include "media/base/key_system_info.h"

namespace cdm {

// Get the list of supported key systems. `can_persist_data` specifies whether
// any data can be persisted by Chrome or by MediaDrm (e.g. should be false in
// incognito mode). `cb` is called with the list of available key systems, and
// may be called multiple times if the list changes (e.g. a new key system
// becomes available).
void GetSupportedKeySystemsUpdates(bool can_persist_data,
                                   media::GetSupportedKeySystemsCB cb);

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_KEY_SYSTEM_SUPPORT_UPDATE_H_
