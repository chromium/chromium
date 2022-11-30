// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

struct COMPONENT_EXPORT(APP_TYPES) Shortcut {
  Shortcut(const std::string& shortcut_id,
           const std::string& name,
           uint8_t position = 0);

  Shortcut(const Shortcut&) = delete;
  Shortcut& operator=(const Shortcut&) = delete;
  Shortcut(Shortcut&&) = default;
  Shortcut& operator=(Shortcut&&) = default;

  ~Shortcut();

  bool operator==(const Shortcut& other) const;
  bool operator!=(const Shortcut& other) const;

  std::unique_ptr<Shortcut> Clone() const;

  // Example output:
  //
  // shortcut_id: 2
  // - shortcut_name: Launch
  // - position: 0
  std::string ToString() const;

  // Represents a particular shortcut in an app. Needs to be unique within an
  // app as calls will be made using both app_id and shortcut_id.
  std::string shortcut_id;
  // Name of the shortcut.
  std::string name;
  // "Position" of a shortcut, which is a non-negative, sequential
  // value. If position is 0, no position was specified.
  uint8_t position;
};

using ShortcutPtr = std::unique_ptr<Shortcut>;
using Shortcuts = std::vector<ShortcutPtr>;

// Creates a deep copy of `source_shortcuts`.
COMPONENT_EXPORT(APP_TYPES)
Shortcuts CloneShortcuts(const Shortcuts& source_shortcuts);

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const Shortcuts& source, const Shortcuts& target);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_H_