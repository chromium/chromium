// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace machine_learning {

// This class serves requests from chrome://machine-learning-internals by
// forwarding them to the ML Service.
class MachineLearningInternalsPageHandler : public mojom::PageHandler {
 public:
  explicit MachineLearningInternalsPageHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver);
  ~MachineLearningInternalsPageHandler() override;

 private:
  // mojom::PageHandler:
  void LoadBuiltinModel(mojom::BuiltinModelSpecPtr spec,
                        mojo::PendingReceiver<mojom::Model> receiver,
                        LoadBuiltinModelCallback callback) override;

  mojo::Receiver<mojom::PageHandler> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MachineLearningInternalsPageHandler);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_PAGE_HANDLER_H_
