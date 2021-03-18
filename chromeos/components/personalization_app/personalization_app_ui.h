// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_
#define CHROMEOS_COMPONENTS_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

class PersonalizationAppUI : public ui::MojoWebUIController {
 public:
  explicit PersonalizationAppUI(content::WebUI* web_ui);
  PersonalizationAppUI(const PersonalizationAppUI&) = delete;
  PersonalizationAppUI& operator=(const PersonalizationAppUI&) = delete;
  ~PersonalizationAppUI() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_
