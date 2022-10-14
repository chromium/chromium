// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/engine.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/ime/public/cpp/rulebased/rules_data.h"

namespace ash {
namespace ime {
namespace rulebased {

Engine::Engine() = default;
Engine::~Engine() = default;

// static
bool Engine::IsImeSupported(const std::string& id) {
  return RulesData::IsIdSupported(id);
}

void Engine::Activate(const std::string& id) {
  if (current_id_ != id) {
    Reset();
    current_id_ = id;
    current_data_ = RulesData::GetById(id);
  }
}

void Engine::Reset() {
  // Clears current state.
  context_ = "";
  transat_ = -1;

  ClearHistory();
}

ProcessKeyResult Engine::ProcessKey(mojom::DomCode code,
                                    uint8_t modifier_state) {
  ProcessKeyResult res;
  // The fallback result should commit the existing composition text.
  res.commit_text = context_;
  if (!current_data_ || modifier_state > 7) {
    Reset();
    return res;
  }

  const KeyMap* key_map = current_data_->GetKeyMapByModifiers(modifier_state);
  auto it = key_map->find(code);
  if (it == key_map->end()) {
    if (code == mojom::DomCode::kBackspace && !context_.empty())
      return ProcessBackspace();
    Reset();
    return res;
  }

  res.key_handled = true;

  const std::string& key_char = it->second;

  if (!current_data_->HasTransform()) {
    res.commit_text = key_char;
    return res;
  }

  // Deals with the transforms.
  std::string composition;
  // If history exists, use history to match first.
  // Otherwise, use current state to match.
  bool matched = false;
  if (!history_ambi_.empty()) {
    matched = current_data_->Transform(history_context_, history_transat_,
                                       history_ambi_ + key_char, &composition);
  }
  if (!matched)
    matched =
        current_data_->Transform(context_, transat_, key_char, &composition);

  // Updates history as necessary before changing the current state.
  if (current_data_->MatchHistoryPrune(history_ambi_ + key_char)) {
    if (history_ambi_.empty()) {  // First time?
      history_context_ = context_;
      history_transat_ = transat_;
      history_ambi_ = key_char;
    } else {
      history_ambi_ += key_char;
    }
  } else if (current_data_->MatchHistoryPrune(key_char)) {
    history_context_ = context_;
    history_transat_ = transat_;
    history_ambi_ = key_char;
  } else {
    ClearHistory();
  }

  // Changing the current state.
  if (matched) {
    context_ = composition;
    transat_ = composition.size();
  } else {
    context_ += key_char;
  }

  if (!current_data_->PredictTransform(context_, transat_) ||
      (!history_ambi_.empty() &&
       !current_data_->PredictTransform(history_context_ + history_ambi_,
                                        history_transat_))) {
    res.commit_text = context_;
    Reset();
    return res;
  }

  // Returns the result according to the current state.
  res.composition_text = context_;
  res.commit_text = "";
  return res;
}

// private
void Engine::ClearHistory() {
  history_context_ = "";
  history_transat_ = -1;
  history_ambi_ = "";
}

ProcessKeyResult Engine::ProcessBackspace() {
  ProcessKeyResult res;
  // Reverts the current state.
  // If the backspace across over the transat pos, adjusts it.
  std::u16string text = base::UTF8ToUTF16(context_);
  text = text.substr(0, text.length() - 1);
  context_ = base::UTF16ToUTF8(text);
  if (transat_ > (int)context_.length())
    transat_ = context_.length();

  // Reverts the history state, clears it if necessary.
  if (!history_ambi_.empty()) {
    text = base::UTF8ToUTF16(history_ambi_);
    text = text.substr(0, text.length() - 1);
    history_ambi_ = base::UTF16ToUTF8(text);
    if (history_ambi_.empty())
      ClearHistory();
  }
  res.key_handled = true;
  res.composition_text = context_;
  return res;
}

}  // namespace rulebased
}  // namespace ime
}  // namespace ash
