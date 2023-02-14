// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/cdm/renderer/key_system_support_update.h"

void GetChromeKeySystems(media::GetSupportedKeySystemsCB cb) {
  cdm::GetSupportedKeySystemsUpdates(
      !ChromeRenderThreadObserver::is_incognito_process(), std::move(cb));
}
