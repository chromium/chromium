// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class FloatingWorkspaceUI;

// The WebUIConfig for the FloatingWorkspaceUI class.
class FloatingWorkspaceUIConfig
    : public ChromeOSWebUIConfig<FloatingWorkspaceUI> {
 public:
  FloatingWorkspaceUIConfig();
};

class FloatingWorkspaceUI : public ui::WebDialogUI {
 public:
  explicit FloatingWorkspaceUI(content::WebUI* web_ui);
  FloatingWorkspaceUI(const FloatingWorkspaceUI&) = delete;
  FloatingWorkspaceUI& operator=(const FloatingWorkspaceUI&) = delete;
  ~FloatingWorkspaceUI() override;

 private:
  base::WeakPtrFactory<FloatingWorkspaceUI> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UI_H_
