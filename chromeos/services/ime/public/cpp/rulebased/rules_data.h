// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_RULES_DATA_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_RULES_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"

namespace chromeos {
namespace ime {
namespace rulebased {

typedef std::map<std::string, std::string> KeyMap;

KeyMap ParseKeyMapForTesting(const wchar_t* raw_key_map, bool is_102);

class RulesData {
 public:
  RulesData();
  ~RulesData();
  static std::unique_ptr<const RulesData> GetById(const std::string& id);
  static bool IsIdSupported(const std::string& id);

  const KeyMap* GetKeyMapByModifiers(uint8_t modifiers) const;

 private:
  std::vector<KeyMap> key_map_cache_;
  const KeyMap* key_maps_[8] = {0};

  DISALLOW_COPY_AND_ASSIGN(RulesData);
};

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_RULES_DATA_H_
