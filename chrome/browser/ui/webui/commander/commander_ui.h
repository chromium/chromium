// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMANDER_COMMANDER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMMANDER_COMMANDER_UI_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui_controller.h"

class CommanderHandler;

// Entry point for the Commander WebUI interface.
class CommanderUI : public content::WebUIController {
 public:
  explicit CommanderUI(content::WebUI* web_ui);
  ~CommanderUI() override;

  // Disallow copy and assign
  CommanderUI(const CommanderUI& other) = delete;
  CommanderUI& operator=(const CommanderUI& other) = delete;

  CommanderHandler* handler() { return handler_; }

 private:
  raw_ptr<CommanderHandler> handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMMANDER_COMMANDER_UI_H_
