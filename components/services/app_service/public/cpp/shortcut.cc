// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut.h"

#include <sstream>

#include "base/check.h"

namespace apps {

Shortcut::Shortcut(const std::string& shortcut_id,
                   const std::string& name,
                   uint8_t position)
    : shortcut_id(shortcut_id), name(name), position(position) {}

Shortcut::~Shortcut() = default;

bool Shortcut::operator==(const Shortcut& other) const {
  return shortcut_id == other.shortcut_id && name == other.name &&
         position == other.position;
}

bool Shortcut::operator!=(const Shortcut& other) const {
  return !(*this == other);
}

std::unique_ptr<Shortcut> Shortcut::Clone() const {
  return std::make_unique<Shortcut>(shortcut_id, name, position);
}

std::string Shortcut::ToString() const {
  std::stringstream out;
  out << "shortcut_id: " << shortcut_id << std::endl;
  out << "- name: " << name << std::endl;
  out << "- position: " << position << std::endl;
  return out.str();
}

Shortcuts CloneShortcuts(const Shortcuts& source_shortcuts) {
  Shortcuts shortcuts;
  for (const auto& shortcut : source_shortcuts) {
    DCHECK(shortcut);
    shortcuts.push_back(shortcut->Clone());
  }
  return shortcuts;
}

bool IsEqual(const Shortcuts& source, const Shortcuts& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }

  return true;
}

}  // namespace apps
