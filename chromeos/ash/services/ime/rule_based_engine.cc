// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/rule_based_engine.h"

#include "base/i18n/icu_string_conversions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace ash {
namespace ime {

namespace {

std::u16string ConvertToUtf16AndNormalize(const std::string& str) {
  // TODO(https://crbug.com/1185629): Add a new helper in
  // base/i18n/icu_string_conversions.h that does the conversion directly
  // without a redundant UTF16->UTF8 conversion.
  std::string normalized_str;
  base::ConvertToUtf8AndNormalize(str, base::kCodepageUTF8, &normalized_str);
  return base::UTF8ToUTF16(normalized_str);
}

std::string GetIdFromImeSpec(const std::string& ime_spec) {
  static const std::string kPrefix("m17n:");
  return base::StartsWith(ime_spec, kPrefix, base::CompareCase::SENSITIVE)
             ? ime_spec.substr(kPrefix.length())
             : std::string();
}

uint8_t GenerateModifierValue(const mojom::ModifierStatePtr& modifier_state,
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

mojom::KeyEventResult HandleEngineResult(
    const rulebased::ProcessKeyResult& result,
    mojo::AssociatedRemote<mojom::InputMethodHost>& host) {
  if (!result.commit_text.empty()) {
    host->CommitText(ConvertToUtf16AndNormalize(result.commit_text),
                     mojom::CommitTextCursorBehavior::kMoveCursorAfterText);
  }
  // Still need to set composition when the key is handled and commit_text and
  // composition_text are both empty.
  // That is the case of using Backspace to delete the last character in
  // composition.
  if (!result.composition_text.empty() ||
      (result.key_handled && result.commit_text.empty())) {
    std::u16string text = ConvertToUtf16AndNormalize(result.composition_text);
    std::vector<mojom::CompositionSpanPtr> spans;
    spans.push_back(mojom::CompositionSpan::New(
        0, text.length(), mojom::CompositionSpanStyle::kDefault));
    const int new_cursor_position = text.length();
    host->SetComposition(std::move(text), std::move(spans),
                         new_cursor_position);
  }
  return result.key_handled ? mojom::KeyEventResult::kConsumedByIme
                            : mojom::KeyEventResult::kNeedsHandlingBySystem;
}

bool IsModifierKey(const mojom::DomCode code) {
  switch (code) {
    case mojom::DomCode::kAltLeft:
    case mojom::DomCode::kAltRight:
    case mojom::DomCode::kShiftLeft:
    case mojom::DomCode::kShiftRight:
    case mojom::DomCode::kControlLeft:
    case mojom::DomCode::kControlRight:
    case mojom::DomCode::kCapsLock:
      return true;
    default:
      return false;
  }
}

// Returns whether the given ime_spec is supported by rulebased engine.
bool IsImeSupportedByRulebased(const std::string& ime_spec) {
  return rulebased::Engine::IsImeSupported(GetIdFromImeSpec(ime_spec));
}

}  // namespace

std::unique_ptr<RuleBasedEngine> RuleBasedEngine::Create(
    const std::string& ime_spec,
    mojo::PendingAssociatedReceiver<mojom::InputMethod> receiver,
    mojo::PendingAssociatedRemote<mojom::InputMethodHost> host) {
  // RuleBasedEngine constructor is private, so have to use WrapUnique here.
  return IsImeSupportedByRulebased(ime_spec)
             ? base::WrapUnique(new RuleBasedEngine(
                   ime_spec, std::move(receiver), std::move(host)))
             : nullptr;
}

RuleBasedEngine::~RuleBasedEngine() = default;

void RuleBasedEngine::OnFocus(mojom::InputFieldInfoPtr input_field_info,
                              mojom::InputMethodSettingsPtr settings,
                              OnFocusCallback callback) {
  std::move(callback).Run(false);
}

bool RuleBasedEngine::IsConnected() {
  // `receiver_` will reset upon disconnection, so bound state is equivalent to
  // connected state.
  return receiver_.is_bound();
}

void RuleBasedEngine::OnCompositionCanceledBySystem() {
  engine_.Reset();
  is_alt_right_key_down_ = false;
}

void RuleBasedEngine::ProcessKeyEvent(mojom::PhysicalKeyEventPtr event,
                                      ProcessKeyEventCallback callback) {
  // According to the W3C spec, |altKey| is false if the AltGr key
  // is pressed [1]. However, all rule-based input methods on Chrome OS use
  // the US QWERTY layout as a base layout, with AltGr implemented at this
  // layer. This means the right Alt key reports as being a normal Alt key, so
  // |altKey| is true. Thus, we need to take |altKey| and exclude the
  // right Alt key to determine the status of the "true" Alt key.
  // [1] https://www.w3.org/TR/uievents-key/#keys-modifier
  // TODO(https://crbug.com/1014778): Change the base layouts for the
  // rule-based input methods so that |altKey| is false when AltGr is pressed.
  if (event->code == mojom::DomCode::kAltRight) {
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
    std::move(callback).Run(mojom::KeyEventResult::kNeedsHandlingBySystem);
    return;
  }

  std::move(callback).Run(HandleEngineResult(
      engine_.ProcessKey(
          event->code,
          GenerateModifierValue(event->modifier_state, is_alt_right_key_down_)),
      host_));
}

void RuleBasedEngine::OnCandidateSelected(uint32_t selected_candidate_index) {
  // Rule-based engines don't use candidates.
  NOTREACHED();
}

void RuleBasedEngine::OnAssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) {
  // Rule-based engines don't use the assistive window.
  NOTREACHED();
}

void RuleBasedEngine::OnQuickSettingsUpdated(
    mojom::InputMethodQuickSettingsPtr quick_settings) {
  // Rule-based engines don't use quick settings.
  NOTREACHED();
}

void RuleBasedEngine::IsReadyForTesting(IsReadyForTestingCallback callback) {
  // Rule-based engines load instantly, so they are always ready.
  std::move(callback).Run(true);
}

RuleBasedEngine::RuleBasedEngine(
    const std::string& ime_spec,
    mojo::PendingAssociatedReceiver<mojom::InputMethod> receiver,
    mojo::PendingAssociatedRemote<mojom::InputMethodHost> host)
    : receiver_(this, std::move(receiver)), host_(std::move(host)) {
  DCHECK(IsImeSupportedByRulebased(ime_spec));

  engine_.Activate(GetIdFromImeSpec(ime_spec));

  receiver_.set_disconnect_handler(
      base::BindOnce(&mojo::AssociatedReceiver<mojom::InputMethod>::reset,
                     base::Unretained(&receiver_)));
}

}  // namespace ime
}  // namespace ash
