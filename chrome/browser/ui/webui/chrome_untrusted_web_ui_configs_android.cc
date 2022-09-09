// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_android.h"

#include "chrome/browser/ui/webui/video_tutorials/video_player_ui.h"
#include "content/public/browser/webui_config_map.h"

void RegisterAndroidChromeUntrustedWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();

  // Add untrusted `WebUIConfig`s for Android to the list here.
  map.AddUntrustedWebUIConfig(
      std::make_unique<video_tutorials::VideoPlayerUIConfig>());
}
