// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CAMERA_CAMERA_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CAMERA_CAMERA_UI_H_

#include "base/macros.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

class CameraUI : public ui::MojoWebUIController {
 public:
  explicit CameraUI(content::WebUI* web_ui);
  ~CameraUI() override;

  // True when the Camera as a System Web App flag is true.
  static bool IsEnabled();

 private:
  DISALLOW_COPY_AND_ASSIGN(CameraUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CAMERA_CAMERA_UI_H_
