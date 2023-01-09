// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/assistant/search_and_assistant_enabled_checker.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

// AssistantSetup is the class responsible for start Assistant OptIn flow.
class AssistantSetup : public ash::AssistantSetup,
                       public ash::AssistantStateObserver,
                       public SearchAndAssistantEnabledChecker::Delegate {
 public:
  AssistantSetup();

  AssistantSetup(const AssistantSetup&) = delete;
  AssistantSetup& operator=(const AssistantSetup&) = delete;

  ~AssistantSetup() override;

  // ash::AssistantSetup:
  void StartAssistantOptInFlow(
      ash::FlowType type,
      StartAssistantOptInFlowCallback callback) override;
  bool BounceOptInWindowIfActive() override;

  // If prefs::kVoiceInteractionConsentStatus is nullptr, means the
  // pref is not set by user. Therefore we need to start OOBE.
  void MaybeStartAssistantOptInFlow();

  // SearchAndAssistantEnabledChecker::Delegate:
  void OnError() override;
  void OnSearchAndAssistantStateReceived(bool is_disabled) override;

 private:
  // ash::AssistantStateObserver:
  void OnAssistantStatusChanged(
      ash::assistant::AssistantStatus status) override;

  void SyncSettingsState();
  void OnGetSettingsResponse(const std::string& settings);

  std::unique_ptr<SearchAndAssistantEnabledChecker>
      search_and_assistant_enabled_checker_;

  base::WeakPtrFactory<AssistantSetup> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
