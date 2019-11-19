// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// AssistantSetup is the class responsible for start Assistant OptIn flow.
class AssistantSetup : public ash::AssistantSetup,
                       public ash::AssistantStateObserver {
 public:
  explicit AssistantSetup(
      chromeos::assistant::mojom::AssistantService* service);
  ~AssistantSetup() override;

  // ash::AssistantSetup:
  void StartAssistantOptInFlow(
      ash::FlowType type,
      StartAssistantOptInFlowCallback callback) override;
  bool BounceOptInWindowIfActive() override;

  // If prefs::kVoiceInteractionConsentStatus is nullptr, means the
  // pref is not set by user. Therefore we need to start OOBE.
  void MaybeStartAssistantOptInFlow();

 private:
  // ash::AssistantStateObserver:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override;

  void SyncSettingsState();
  void OnGetSettingsResponse(const std::string& settings);

  chromeos::assistant::mojom::AssistantService* const service_;
  mojo::Remote<chromeos::assistant::mojom::AssistantSettingsManager>
      settings_manager_;

  base::WeakPtrFactory<AssistantSetup> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantSetup);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
