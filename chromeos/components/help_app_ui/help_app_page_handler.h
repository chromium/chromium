// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_PAGE_HANDLER_H_
#define CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_PAGE_HANDLER_H_

#include "chromeos/components/help_app_ui/help_app_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
class HelpAppUI;
}

// Implements the help_app mojom interface providing chrome://help-app
// with browser process functions to call from the render process.
class HelpAppPageHandler : public help_app_ui::mojom::PageHandler {
 public:
  HelpAppPageHandler(
      chromeos::HelpAppUI* help_app_ui,
      mojo::PendingReceiver<help_app_ui::mojom::PageHandler> receiver);
  ~HelpAppPageHandler() override;

  HelpAppPageHandler(const HelpAppPageHandler&) = delete;
  HelpAppPageHandler& operator=(const HelpAppPageHandler&) = delete;

  // help_app_ui::mojom::PageHandler:
  void OpenFeedbackDialog(OpenFeedbackDialogCallback callback) override;
  void ShowParentalControls() override;
  void IsLssEnabled(IsLssEnabledCallback callback) override;

 private:
  mojo::Receiver<help_app_ui::mojom::PageHandler> receiver_;
  chromeos::HelpAppUI* help_app_ui_;  // Owns |this|.
  bool is_lss_enabled_;
};

#endif  // CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_PAGE_HANDLER_H_
