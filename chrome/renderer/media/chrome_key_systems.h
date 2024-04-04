// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CHROME_KEY_SYSTEMS_H_
#define CHROME_RENDERER_MEDIA_CHROME_KEY_SYSTEMS_H_

#include "content/public/renderer/render_frame.h"
#include "media/base/key_system_info.h"
#include "media/base/key_systems_support_registration.h"

// Register the key systems supported by the chrome/ layer.
std::unique_ptr<media::KeySystemSupportRegistration> GetChromeKeySystems(
    content::RenderFrame* render_frame,
    media::GetSupportedKeySystemsCB cb);

#endif  // CHROME_RENDERER_MEDIA_CHROME_KEY_SYSTEMS_H_
