// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_H_
#define CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_H_

#include "base/macros.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// The WebUI controller for chrome://media-app.
class MediaAppUI : public ui::MojoWebUIController {
 public:
  explicit MediaAppUI(content::WebUI* web_ui);
  ~MediaAppUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaAppUI);
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_H_
