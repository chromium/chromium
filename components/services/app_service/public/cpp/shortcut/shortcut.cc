// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

#include <memory>
#include <sstream>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "components/app_constants/constants.h"
#include "components/crx_file/id_util.h"

namespace apps {

APP_ENUM_TO_STRING(ShortcutSource, kUnknown, kUser, kPolicy, kDefault)

Shortcut::Shortcut(const std::string& host_app_id, const std::string& local_id)
    : host_app_id(host_app_id),
      local_id(local_id),
      shortcut_id(GenerateShortcutId(host_app_id, local_id)) {}

Shortcut::~Shortcut() = default;

bool Shortcut::operator==(const Shortcut& rhs) const {
  return this->shortcut_id == rhs.shortcut_id &&
         this->host_app_id == rhs.host_app_id &&
         this->local_id == rhs.local_id && this->name == rhs.name &&
         this->shortcut_source == rhs.shortcut_source &&
         this->icon_key == rhs.icon_key &&
         this->allow_removal == rhs.allow_removal;
}

std::unique_ptr<Shortcut> Shortcut::Clone() const {
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);

  shortcut->name = name;
  shortcut->shortcut_source = shortcut_source;
  if (icon_key.has_value()) {
    shortcut->icon_key = std::move(*icon_key->Clone());
  }
  shortcut->allow_removal = allow_removal;
  return shortcut;
}

std::string Shortcut::ToString() const {
  std::stringstream out;
  out << "shortcut_id: " << shortcut_id << std::endl;
  if (name.has_value()) {
    out << "- name: " << name.value() << std::endl;
  }
  out << "- shortcut_source: " << EnumToString(shortcut_source) << std::endl;
  out << "- host_app_id: " << host_app_id << std::endl;
  out << "- local_id: " << local_id << std::endl;
  if (allow_removal.has_value()) {
    out << "- allow_removal: " << allow_removal.value() << std::endl;
  }
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

ShortcutId GenerateShortcutId(const std::string& host_app_id,
                              const std::string& local_id) {
  // For web app based browser shortcut, we just use the local_id
  // that is generated in the web app system, so that we can keep
  // all the launcher and shelf locations without needing to migrate the sync
  // data.
  if (host_app_id == app_constants::kChromeAppId ||
      host_app_id == app_constants::kLacrosAppId) {
    return ShortcutId(local_id);
  }
  const std::string input = base::StrCat({host_app_id, "#", local_id});
  return ShortcutId(crx_file::id_util::GenerateId(input));
}

}  // namespace apps
