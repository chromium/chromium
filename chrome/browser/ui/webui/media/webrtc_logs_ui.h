// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_LOGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_LOGS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class WebRtcLogsUI;

class WebRtcLogsUIConfig : public content::DefaultWebUIConfig<WebRtcLogsUI> {
 public:
  WebRtcLogsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIWebRtcLogsHost) {}
};

// The WebUI handler for chrome://webrtc-logs.
class WebRtcLogsUI : public content::WebUIController {
 public:
  explicit WebRtcLogsUI(content::WebUI* web_ui);

  WebRtcLogsUI(const WebRtcLogsUI&) = delete;
  WebRtcLogsUI& operator=(const WebRtcLogsUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_LOGS_UI_H_
