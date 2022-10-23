// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_RULE_BASED_ENGINE_H_
#define CHROMEOS_ASH_SERVICES_IME_RULE_BASED_ENGINE_H_

#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/ash/services/ime/public/cpp/rulebased/engine.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace ash {
namespace ime {

// Handles rule-based input methods such as Arabic and Vietnamese.
// Rule-based input methods are based off deterministic rules and do not
// provide features such as suggestions.
class RuleBasedEngine : public mojom::InputMethod {
 public:
  // Returns nullptr if |ime_spec| is not valid for this RuleBasedEngine.
  static std::unique_ptr<RuleBasedEngine> Create(
      const std::string& ime_spec,
      mojo::PendingAssociatedReceiver<mojom::InputMethod> receiver,
      mojo::PendingAssociatedRemote<mojom::InputMethodHost> host);

  RuleBasedEngine(const RuleBasedEngine& other) = delete;
  RuleBasedEngine& operator=(const RuleBasedEngine& other) = delete;
  ~RuleBasedEngine() override;

  bool IsConnected();

  // mojom::InputMethod overrides:
  // Most of these methods are deliberately empty because rule-based input
  // methods do not need to listen to these events.
  void OnFocusDeprecated(mojom::InputFieldInfoPtr input_field_info,
                         mojom::InputMethodSettingsPtr settings) override {}
  void OnFocus(mojom::InputFieldInfoPtr input_field_info,
               mojom::InputMethodSettingsPtr settings,
               OnFocusCallback callback) override;
  void OnBlur() override {}
  void OnSurroundingTextChanged(
      const std::string& text,
      uint32_t offset,
      mojom::SelectionRangePtr selection_range) override {}
  void OnCompositionCanceledBySystem() override;
  void ProcessKeyEvent(mojom::PhysicalKeyEventPtr event,
                       ProcessKeyEventCallback callback) override;
  void OnCandidateSelected(uint32_t selected_candidate_index) override;
  void OnAssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) override;
  void OnQuickSettingsUpdated(
      mojom::InputMethodQuickSettingsPtr quick_settings) override;
  void IsReadyForTesting(IsReadyForTestingCallback callback) override;

  // TODO(https://crbug.com/837156): Implement a state for the interface.

 private:
  RuleBasedEngine(const std::string& ime_spec,
                  mojo::PendingAssociatedReceiver<mojom::InputMethod> receiver,
                  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host);

  mojo::AssociatedReceiver<mojom::InputMethod> receiver_;
  mojo::AssociatedRemote<mojom::InputMethodHost> host_;

  rulebased::Engine engine_;

  // Whether the AltRight key is held down or not.
  bool is_alt_right_key_down_ = false;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_RULE_BASED_ENGINE_H_
