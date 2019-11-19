// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_state_proxy.h"

#include <algorithm>
#include <utility>

#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

AssistantStateProxy::AssistantStateProxy() = default;

AssistantStateProxy::~AssistantStateProxy() {
  // Reset pref change registar.
  RegisterPrefChanges(nullptr);
}

void AssistantStateProxy::Init(mojom::ClientProxy* client,
                               PrefService* profile_prefs) {
  // Bind to AssistantStateController.
  mojo::PendingRemote<ash::mojom::AssistantStateController> remote_controller;
  client->RequestAssistantStateController(
      remote_controller.InitWithNewPipeAndPassReceiver());
  assistant_state_controller_.Bind(std::move(remote_controller));

  mojo::PendingRemote<ash::mojom::AssistantStateObserver> observer;
  assistant_state_observer_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver());
  assistant_state_controller_->AddMojomObserver(std::move(observer));

  pref_service_ = profile_prefs;
  RegisterPrefChanges(pref_service_);
}

void AssistantStateProxy::OnAssistantStatusChanged(
    ash::mojom::AssistantState state) {
  UpdateAssistantStatus(state);
}

void AssistantStateProxy::OnAssistantFeatureAllowedChanged(
    ash::mojom::AssistantAllowedState state) {
  UpdateFeatureAllowedState(state);
}

void AssistantStateProxy::OnLocaleChanged(const std::string& locale) {
  UpdateLocale(locale);
}

void AssistantStateProxy::OnArcPlayStoreEnabledChanged(bool enabled) {
  UpdateArcPlayStoreEnabled(enabled);
}

void AssistantStateProxy::OnLockedFullScreenStateChanged(bool enabled) {
  UpdateLockedFullScreenState(enabled);
}

}  // namespace assistant
}  // namespace chromeos
