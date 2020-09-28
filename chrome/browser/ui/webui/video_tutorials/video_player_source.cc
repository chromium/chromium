// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/video_tutorials/video_player_source.h"

#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace video_tutorials {

// static
content::WebUIDataSource* CreateVideoPlayerUntrustedDataSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
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
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources/ 'self';");

  return source;
}

}  // namespace video_tutorials
