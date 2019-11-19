// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {
namespace machine_learning {

class MachineLearningInternalsPageHandler;

// UI controller for machine learning internals page
class MachineLearningInternalsUI : public ui::MojoWebUIController {
 public:
  explicit MachineLearningInternalsUI(content::WebUI* web_ui);
  ~MachineLearningInternalsUI() override;

 private:
  void BindMachineLearningInternalsPageHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver);

  std::unique_ptr<MachineLearningInternalsPageHandler> page_handler_;

  DISALLOW_COPY_AND_ASSIGN(MachineLearningInternalsUI);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_UI_H_
