// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEBUG_DEBUG_OVERLAY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEBUG_DEBUG_OVERLAY_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

namespace ash {

class DebugOverlayHandler : public BaseWebUIHandler {
 public:
  DebugOverlayHandler();
  ~DebugOverlayHandler() override;
  DebugOverlayHandler(const DebugOverlayHandler&) = delete;
  DebugOverlayHandler& operator=(const DebugOverlayHandler&) = delete;

  // BaseWebUIHandler:
  void DeclareJSCallbacks() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  // JS callbacks.
  void HandleCaptureScreenshot(const std::string& name);
  void ToggleColorMode();
  void HandleSwitchWallpaper(const std::string& color);

  base::FilePath screenshot_dir_;
  int screenshot_index_ = 0;
  bool add_resolution_to_filename_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEBUG_DEBUG_OVERLAY_HANDLER_H_
