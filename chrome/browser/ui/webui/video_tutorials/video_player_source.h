// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VIDEO_TUTORIALS_VIDEO_PLAYER_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_VIDEO_TUTORIALS_VIDEO_PLAYER_SOURCE_H_

#include "content/public/browser/web_ui_data_source.h"

namespace video_tutorials {

// The data source creation for chrome-untrusted://video-tutorials/.
content::WebUIDataSource* CreateVideoPlayerUntrustedDataSource();

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_UI_WEBUI_VIDEO_TUTORIALS_VIDEO_PLAYER_SOURCE_H_
