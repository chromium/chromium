// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_AUDIO_AUDIO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_AUDIO_AUDIO_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://audio
class AudioUI : public content::WebUIController {
 public:
  explicit AudioUI(content::WebUI* web_ui);
  AudioUI(const AudioUI&) = delete;
  AudioUI& operator=(const AudioUI&) = delete;
  ~AudioUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_AUDIO_AUDIO_UI_H_
