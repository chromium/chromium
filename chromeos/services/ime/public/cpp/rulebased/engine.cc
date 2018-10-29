// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/engine.h"

#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"

namespace chromeos {
namespace ime {
namespace rulebased {

Engine::Engine() : process_key_count_(0) {}
Engine::~Engine() {}

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
  // TODO(shuchen): Implement this for the ones with transform rules.
  process_key_count_ = 0;
}

ProcessKeyResult Engine::ProcessKey(const std::string& code,
                                    uint8_t modifier_state) {
  process_key_count_++;

  ProcessKeyResult res;
  if (!current_data_)
    return res;
  if (modifier_state > 7)
    return res;

  const KeyMap* key_map = current_data_->GetKeyMapByModifiers(modifier_state);
  auto it = key_map->find(code);
  if (it == key_map->end())
    return res;

  res.key_handled = true;
  res.commit_text = it->second;
  return res;
}

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos
