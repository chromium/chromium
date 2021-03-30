// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VIDEO_TUTORIALS_VIDEO_PLAYER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_VIDEO_TUTORIALS_VIDEO_PLAYER_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

namespace content {
class WebUI;
}  // namespace content

namespace video_tutorials {

class VideoPlayerUIConfig : public ui::WebUIConfig {
 public:
  VideoPlayerUIConfig();
  ~VideoPlayerUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

class VideoPlayerUI : public ui::UntrustedWebUIController {
 public:
  explicit VideoPlayerUI(content::WebUI* web_ui);
  VideoPlayerUI(const VideoPlayerUI&) = delete;
  VideoPlayerUI& operator=(const VideoPlayerUI&) = delete;
  ~VideoPlayerUI() override;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_UI_WEBUI_VIDEO_TUTORIALS_VIDEO_PLAYER_UI_H_
