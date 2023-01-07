// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CHROME_KEY_SYSTEMS_H_
#define CHROME_RENDERER_MEDIA_CHROME_KEY_SYSTEMS_H_

#include <memory>
#include <vector>

#include "media/base/key_system_info.h"

// Register the key systems supported by the chrome/ layer.
void GetChromeKeySystems(media::GetSupportedKeySystemsCB cb);

#endif  // CHROME_RENDERER_MEDIA_CHROME_KEY_SYSTEMS_H_
