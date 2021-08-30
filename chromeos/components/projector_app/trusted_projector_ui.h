// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROJECTOR_APP_TRUSTED_PROJECTOR_UI_H_
#define CHROMEOS_COMPONENTS_PROJECTOR_APP_TRUSTED_PROJECTOR_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class GURL;

namespace chromeos {

// The implementation for the Projector selfie cam and player app WebUI.
// TODO(b/193670945): Migrate to ash/components and ash/webui.
class TrustedProjectorUI : public ui::MojoBubbleWebUIController {
 public:
  TrustedProjectorUI(content::WebUI* web_ui, const GURL& url);
  ~TrustedProjectorUI() override;
  TrustedProjectorUI(const TrustedProjectorUI&) = delete;
  TrustedProjectorUI& operator=(const TrustedProjectorUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PROJECTOR_APP_TRUSTED_PROJECTOR_UI_H_
