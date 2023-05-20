// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/types/strong_alias.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

using ShortcutId = base::StrongAlias<class ShortcutIdTag, std::string>;

// Where the shortcut was created from.
ENUM_FOR_COMPONENT(SHORTCUT,
                   ShortcutSource,
                   kUnknown = 0,
                   kUser = 1,      // Created by the user.
                   kDeveloper = 2  // Created by the developer. e.g. jumplist
)

struct COMPONENT_EXPORT(SHORTCUT) Shortcut {
  Shortcut(const std::string& host_app_id, const std::string& local_id);

  Shortcut(const Shortcut&) = delete;
  Shortcut& operator=(const Shortcut&) = delete;
  Shortcut(Shortcut&&) = delete;
  Shortcut& operator=(Shortcut&&) = delete;

  ~Shortcut();

  std::unique_ptr<Shortcut> Clone() const;

  // Example output:
  //
  // shortcut_id: 2
  // - shortcut_name: Launch
  // - source: Source::kUser
  // - host_app_id: app_1
  // - local_id: shortcut_1
  std::string ToString() const;
  // Name of the shortcut.
  std::string name;
  // Shortcut creation source.
  ShortcutSource shortcut_source;

  // 'host_app_id' and 'local_id' should not be changeable after creation.
  // The host app of the shortcut.
  const std::string host_app_id;
  // The locally unique identifier for the shortcut within an app. This id would
  // be used to launch the shortcut or load shortcut icon from the app.
  const std::string local_id;

  // Represents the unique identifier for a shortcut. This identifier should be
  // unique within a profile, and stable across different user sessions.
  // 'shortcut_id' is generated from the hash of 'host_app_id' and 'local_id',
  // these value should not be updated separately.
  const ShortcutId shortcut_id;
};

using ShortcutPtr = std::unique_ptr<Shortcut>;
using Shortcuts = std::vector<ShortcutPtr>;

// Creates a deep copy of `source_shortcuts`.
COMPONENT_EXPORT(SHORTCUT)
Shortcuts CloneShortcuts(const Shortcuts& source_shortcuts);

COMPONENT_EXPORT(SHORTCUT)
ShortcutId GenerateShortcutId(const std::string& host_app_id,
                              const std::string& local_id);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_H_
