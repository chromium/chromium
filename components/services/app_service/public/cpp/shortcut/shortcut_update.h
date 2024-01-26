// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_UPDATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

namespace apps {

class COMPONENT_EXPORT(SHORTCUT) ShortcutUpdate {
 public:
  static void Merge(Shortcut* state, const Shortcut* delta);

  // At most one of |state| or |delta| may be nullptr.
  ShortcutUpdate(const Shortcut* state, const Shortcut* delta);

  ShortcutUpdate(const ShortcutUpdate&) = delete;
  ShortcutUpdate& operator=(const ShortcutUpdate&) = delete;

  bool operator==(const ShortcutUpdate&) const;

  const ShortcutId& ShortcutId() const;
  const std::string& HostAppId() const;
  const std::string& LocalId() const;

  const std::string& Name() const;
  bool NameChanged() const;

  ShortcutSource ShortcutSource() const;
  bool ShortcutSourceChanged() const;

  std::optional<apps::IconKey> IconKey() const;
  bool IconKeyChanged() const;

  std::optional<bool> AllowRemoval() const;
  bool AllowRemovalChanged() const;

  // Return true if this is a newly registered shortcut in
  // App Service. This could happen when new shortcut created
  // or the shortcut got published to the App Service on
  // start up.
  bool ShortcutInitialized() const;

 private:
  raw_ptr<const Shortcut, DanglingUntriaged> state_ = nullptr;
  raw_ptr<const Shortcut, DanglingUntriaged> delta_ = nullptr;
};

// For logging and debug purposes.
COMPONENT_EXPORT(SHORTCUT)
std::ostream& operator<<(std::ostream& out,
                         const ShortcutUpdate& shortcut_update);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHORTCUT_SHORTCUT_UPDATE_H_
