// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_MANAGER_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_MANAGER_H_

#include <memory>

#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace assistant {

// Interface class that defines assistant settings functionalities.
class AssistantSettingsManager : public mojom::AssistantSettingsManager {
 public:
  ~AssistantSettingsManager() override = default;

  virtual void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantSettingsManager> receiver) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_MANAGER_H_
