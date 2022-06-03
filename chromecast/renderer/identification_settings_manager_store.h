// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_IDENTIFICATION_SETTINGS_MANAGER_STORE_H_
#define CHROMECAST_RENDERER_IDENTIFICATION_SETTINGS_MANAGER_STORE_H_

#include "base/memory/scoped_refptr.h"

namespace chromecast {

class IdentificationSettingsManager;

namespace shell {

class IdentificationSettingsManagerStore {
 public:
  virtual ~IdentificationSettingsManagerStore() = default;

  virtual scoped_refptr<IdentificationSettingsManager>
  GetSettingsManagerFromRenderFrameID(int render_frame_id) = 0;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_IDENTIFICATION_SETTINGS_MANAGER_STORE_H_
