// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROJECTOR_PROJECTOR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROJECTOR_PROJECTOR_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace chromeos {

// The implementation for the Projector Selfie Cam WebUI.
class ProjectorUI : public ui::MojoBubbleWebUIController {
 public:
  explicit ProjectorUI(content::WebUI* web_ui);
  ~ProjectorUI() override;
  ProjectorUI(const ProjectorUI&) = delete;
  ProjectorUI& operator=(const ProjectorUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROJECTOR_PROJECTOR_UI_H_
