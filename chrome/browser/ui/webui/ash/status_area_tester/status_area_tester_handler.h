// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_TESTER_STATUS_AREA_TESTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_TESTER_STATUS_AREA_TESTER_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

// WebUI message handler for chrome://status-area-tester from the Chrome page to
// the System UI.
class StatusAreaTesterHandler : public content::WebUIMessageHandler {
 public:
  StatusAreaTesterHandler();
  StatusAreaTesterHandler(const StatusAreaTesterHandler&) = delete;
  StatusAreaTesterHandler& operator=(const StatusAreaTesterHandler&) = delete;
  ~StatusAreaTesterHandler() override;

  // Handler names
  static const char kToggleIme[];
  static const char kTogglePalette[];

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void SetWebUiForTesting(content::WebUI* web_ui);

 private:
  // Callbacks for events coming from the web UI.
  void ToggleImeTray(const base::Value::List& args);
  void TogglePaletteTray(const base::Value::List& args);

  base::WeakPtrFactory<StatusAreaTesterHandler> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_TESTER_STATUS_AREA_TESTER_HANDLER_H_
