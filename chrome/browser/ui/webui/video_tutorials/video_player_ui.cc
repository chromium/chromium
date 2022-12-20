// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/video_tutorials/video_player_ui.h"

#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace video_tutorials {

VideoPlayerUIConfig::VideoPlayerUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedVideoTutorialsHost) {}

VideoPlayerUIConfig::~VideoPlayerUIConfig() = default;

std::unique_ptr<content::WebUIController>
VideoPlayerUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<VideoPlayerUI>(web_ui);
}

VideoPlayerUI::VideoPlayerUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedVideoPlayerUrl);
  source->AddResourcePath("", IDR_VIDEO_PLAYER_HTML);
  source->AddResourcePath("video_player.css", IDR_VIDEO_PLAYER_CSS);
  source->AddResourcePath("video_player.js", IDR_VIDEO_PLAYER_JS);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc, "connect-src https:;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src https:;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc, "media-src https:;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc, "style-src 'self';");
}

VideoPlayerUI::~VideoPlayerUI() = default;

}  // namespace video_tutorials
