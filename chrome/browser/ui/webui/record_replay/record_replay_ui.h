// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_UI_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

// WebUIController for chrome://recordreplay/.
class RecordReplayUI : public ui::MojoWebUIController {
 public:
  RecordReplayUI(content::WebUI* web_ui);
  RecordReplayUI(const RecordReplayUI&) = delete;
  RecordReplayUI& operator=(const RecordReplayUI&) = delete;
  ~RecordReplayUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_UI_H_
