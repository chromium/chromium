// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

#include <sstream>

#include "base/check.h"

namespace apps {

APP_ENUM_TO_STRING(ShortcutSource, kUnknown, kUser, kDeveloper)

Shortcut::Shortcut(const ShortcutId& shortcut_id) : shortcut_id(shortcut_id) {}

Shortcut::~Shortcut() = default;

Shortcut::Shortcut(Shortcut&&) = default;
Shortcut& Shortcut::operator=(Shortcut&&) = default;

std::unique_ptr<Shortcut> Shortcut::Clone() const {
  auto shortcut = std::make_unique<Shortcut>(shortcut_id);

  shortcut->name = name;
  shortcut->shortcut_source = shortcut_source;
  shortcut->host_app_id = host_app_id;
  shortcut->local_id = local_id;

  return shortcut;
}

std::string Shortcut::ToString() const {
  std::stringstream out;
  out << "shortcut_id: " << shortcut_id << std::endl;
  out << "- name: " << name << std::endl;
  out << "- shortcut_source: " << EnumToString(shortcut_source) << std::endl;
  out << "- host_app_id: " << host_app_id << std::endl;
  out << "- local_id: " << local_id << std::endl;
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

}  // namespace apps
