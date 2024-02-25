// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/strong_alias.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

using ShortcutId = base::StrongAlias<class ShortcutIdTag, std::string>;

// Where the shortcut was created from.
ENUM_FOR_COMPONENT(
    SHORTCUT,
    ShortcutSource,
    kUnknown = 0,
    kUser = 1,    // Created by the user and managed by sync. This includes any
                  // shortcuts created by syncing between devices.
    kPolicy = 2,  // Created by organization policy.
    kDefault = 3  // Created by default.
)

struct COMPONENT_EXPORT(SHORTCUT) Shortcut {
  Shortcut(const std::string& host_app_id, const std::string& local_id);

  Shortcut(const Shortcut&) = delete;
  Shortcut& operator=(const Shortcut&) = delete;
  Shortcut(Shortcut&&) = delete;
  Shortcut& operator=(Shortcut&&) = delete;

  bool operator==(const Shortcut&) const;

  ~Shortcut();

  std::unique_ptr<Shortcut> Clone() const;

  // Example output:
  //
  // shortcut_id: 2
  // - shortcut_name: Launch
  // - source: Source::kUser
  // - host_app_id: app_1
  // - local_id: shortcut_1
  // - allow_removal: true
  std::string ToString() const;
  // Name of the shortcut.
  std::optional<std::string> name;
  // Shortcut creation source.
  ShortcutSource shortcut_source = ShortcutSource::kUnknown;

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

  // Represents what icon should be loaded for this shortcut, icon key will
  // change if the icon has been updated from the publisher.
  std::optional<IconKey> icon_key;

  // Whether the shortcut publisher allows the shortcut to be removed by user.
  std::optional<bool> allow_removal;
};

// A view class to reduce the risk of lifetime issues by preventing
// long-term storage on the heap.
class COMPONENT_EXPORT(SHORTCUT) ShortcutView {
 public:
  explicit ShortcutView(const Shortcut* shortcut) : shortcut_(shortcut) {}
  const Shortcut* operator->() const { return shortcut_.get(); }
  explicit operator bool() const { return shortcut_; }

 private:
  const raw_ptr<const Shortcut> shortcut_;
  STACK_ALLOCATED();
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
