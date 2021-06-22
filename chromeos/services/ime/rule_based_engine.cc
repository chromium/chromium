// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/rule_based_engine.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace chromeos {
namespace ime {

namespace {

std::string GetIdFromImeSpec(const std::string& ime_spec) {
  static const std::string kPrefix("m17n:");
  return base::StartsWith(ime_spec, kPrefix, base::CompareCase::SENSITIVE)
             ? ime_spec.substr(kPrefix.length())
             : std::string();
}

uint8_t GenerateModifierValueForRulebased(
    const mojom::ModifierStatePtr& modifier_state,
    bool is_alt_right_key_down) {
  uint8_t modifiers = 0;
  if (modifier_state->shift)
    modifiers |= rulebased::MODIFIER_SHIFT;
  if (modifier_state->alt_graph || is_alt_right_key_down)
    modifiers |= rulebased::MODIFIER_ALTGR;
  if (modifier_state->caps_lock)
    modifiers |= rulebased::MODIFIER_CAPSLOCK;
  return modifiers;
}

mojom::KeypressResponseForRulebasedPtr GenerateKeypressResponseForRulebased(
    rulebased::ProcessKeyResult& process_key_result) {
  mojom::KeypressResponseForRulebasedPtr keypress_response =
      mojom::KeypressResponseForRulebased::New();
  keypress_response->result = process_key_result.key_handled;
  if (!process_key_result.commit_text.empty()) {
    keypress_response->operations.push_back(mojom::OperationForRulebased::New(
        mojom::OperationMethodForRulebased::COMMIT_TEXT,
        process_key_result.commit_text));
  }
  // Need to add the setComposition operation to the result when the key is
  // handled and commit_text and composition_text are both empty.
  // That is the case of using Backspace to delete the last character in
  // composition.
  if (!process_key_result.composition_text.empty() ||
      (process_key_result.key_handled &&
       process_key_result.commit_text.empty())) {
    keypress_response->operations.push_back(mojom::OperationForRulebased::New(
        mojom::OperationMethodForRulebased::SET_COMPOSITION,
        process_key_result.composition_text));
  }
  return keypress_response;
}

bool IsModifierKey(const std::string& key_code) {
  return key_code == "AltLeft" || key_code == "AltRight" ||
         key_code == "ShiftLeft" || key_code == "ShiftRight" ||
         key_code == "ControlLeft" || key_code == "ControlRight" ||
         key_code == "CapsLock";
}

// Returns whether the given ime_spec is supported by rulebased engine.
bool IsImeSupportedByRulebased(const std::string& ime_spec) {
  return rulebased::Engine::IsImeSupported(GetIdFromImeSpec(ime_spec));
}

}  // namespace

std::unique_ptr<RuleBasedEngine> RuleBasedEngine::Create(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> receiver) {
  // RuleBasedEngine constructor is private, so have to use WrapUnique here.
  return IsImeSupportedByRulebased(ime_spec)
             ? base::WrapUnique(
                   new RuleBasedEngine(ime_spec, std::move(receiver)))
             : nullptr;
}

RuleBasedEngine::~RuleBasedEngine() = default;

void RuleBasedEngine::OnCompositionCanceledBySystem() {
  engine_.Reset();
  is_alt_right_key_down_ = false;
}

void RuleBasedEngine::ProcessKeypressForRulebased(
    mojom::PhysicalKeyEventPtr event,
    ProcessKeypressForRulebasedCallback callback) {
  // According to the W3C spec, |altKey| is false if the AltGr key
  // is pressed [1]. However, all rule-based input methods on Chrome OS use
  // the US QWERTY layout as a base layout, with AltGr implemented at this
  // layer. This means the right Alt key reports as being a normal Alt key, so
  // |altKey| is true. Thus, we need to take |altKey| and exclude the
  // right Alt key to determine the status of the "true" Alt key.
  // [1] https://www.w3.org/TR/uievents-key/#keys-modifier
  // TODO(https://crbug.com/1014778): Change the base layouts for the
  // rule-based input methods so that |altKey| is false when AltGr is pressed.
  if (event->code == "AltRight") {
    is_alt_right_key_down_ = event->type == mojom::KeyEventType::kKeyDown;
  }

  const bool is_alt_down = event->modifier_state->alt && !is_alt_right_key_down_;

  // - Shift/AltRight/Caps/Ctrl are modifier keys for the characters which the
  // Mojo service may accept, but don't send the keys themselves to Mojo.
  // - Ctrl+? and Alt+? are shortcut keys, so don't send them to the rule based
  // engine.
  if (event->type != mojom::KeyEventType::kKeyDown ||
      (IsModifierKey(event->code) || event->modifier_state->control ||
       is_alt_down)) {
    std::move(callback).Run(mojom::KeypressResponseForRulebased::New(
        false, std::vector<mojom::OperationForRulebasedPtr>(0)));
    return;
  }

  rulebased::ProcessKeyResult process_key_result = engine_.ProcessKey(
      event->code, GenerateModifierValueForRulebased(event->modifier_state,
                                                     is_alt_right_key_down_));
  mojom::KeypressResponseForRulebasedPtr keypress_response =
      GenerateKeypressResponseForRulebased(process_key_result);

  std::move(callback).Run(std::move(keypress_response));
}

RuleBasedEngine::RuleBasedEngine(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> receiver)
    : receiver_(this, std::move(receiver)) {
  DCHECK(IsImeSupportedByRulebased(ime_spec));

  engine_.Activate(GetIdFromImeSpec(ime_spec));

  // TODO(https://crbug.com/837156): Registry connection error handler.
}

}  // namespace ime
}  // namespace chromeos
