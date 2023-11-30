// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/record_replay/record_replay_manager.mojom-forward.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

class RecordReplayManagerHandler;

// WebUIController for chrome://recordreplay/.
class RecordReplayUI : public ui::MojoWebUIController {
 public:
  RecordReplayUI(content::WebUI* web_ui);
  RecordReplayUI(const RecordReplayUI&) = delete;
  RecordReplayUI& operator=(const RecordReplayUI&) = delete;
  ~RecordReplayUI() override;

  // Instantiates the implementor of the mojom::RecordReplayManagerHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::RecordReplayManagerHandler> receiver);

 private:
  std::unique_ptr<RecordReplayManagerHandler> record_replay_manager_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_UI_H_
