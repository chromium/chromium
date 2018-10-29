// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_ENGINE_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_ENGINE_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace chromeos {
namespace ime {
namespace rulebased {

class RulesData;

enum Modifiers {
  MODIFIER_NONE = 0,
  MODIFIER_SHIFT = 1 << 0,
  MODIFIER_ALTGR = 1 << 1,
  MODIFIER_CAPSLOCK = 1 << 2,
};

struct ProcessKeyResult {
  bool key_handled = false;
  std::string commit_text;
};

class Engine {
 public:
  Engine();
  ~Engine();

  static bool IsImeSupported(const std::string& id);

  void Activate(const std::string& id);
  void Reset();
  ProcessKeyResult ProcessKey(const std::string& code, uint8_t modifier_state);

  uint32_t process_key_count() const { return process_key_count_; }

 private:
  std::unique_ptr<const RulesData> current_data_;
  std::string current_id_;
  uint32_t process_key_count_;

  DISALLOW_COPY_AND_ASSIGN(Engine);
};

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_ENGINE_H_
