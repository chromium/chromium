// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_STATE_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_STATE_PROXY_H_

#include <string>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace chromeos {
namespace assistant {

// Provides a convenient client to access various Assistant states. The state
// information can be accessed through direct accessors which returns
// |base::Optional<>| or observers. When adding an observer, all change events
// will fire if this client already have data.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantStateProxy
    : public ash::AssistantStateBase,
      public ash::mojom::AssistantStateObserver {
 public:
  AssistantStateProxy();
  ~AssistantStateProxy() override;

  void Init(mojom::ClientProxy* client, PrefService* profile_prefs);

 private:
  // AssistantStateObserver:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override;
  void OnAssistantFeatureAllowedChanged(
      ash::mojom::AssistantAllowedState state) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  mojo::Remote<ash::mojom::AssistantStateController>
      assistant_state_controller_;
  mojo::Receiver<ash::mojom::AssistantStateObserver>
      assistant_state_observer_receiver_{this};

  PrefService* pref_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantStateProxy);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_STATE_PROXY_H_
