// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include "chrome/renderer/process_state.h"
#include "components/cdm/renderer/key_system_support_update.h"
#include "content/public/renderer/render_frame.h"
#include "media/base/key_systems_support_registration.h"

std::unique_ptr<media::KeySystemSupportRegistration> GetChromeKeySystems(
    content::RenderFrame* render_frame,
    media::GetSupportedKeySystemsCB cb) {
  return cdm::GetSupportedKeySystemsUpdates(render_frame, !IsIncognitoProcess(),
                                            std::move(cb));
}
