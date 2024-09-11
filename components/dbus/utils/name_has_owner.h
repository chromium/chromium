// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_NAME_HAS_OWNER_H_
#define COMPONENTS_DBUS_UTILS_NAME_HAS_OWNER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}

namespace dbus_utils {

using NameHasOwnerCallback =
    base::OnceCallback<void(/*name_has_owner=*/std::optional<bool>)>;

// Checks whether the service `name` has an owner on the bus and runs `callback`
// with this boolean value. Errors are indicated as a nullopt.
COMPONENT_EXPORT(DBUS)
void NameHasOwner(dbus::Bus* bus,
                  const std::string& name,
                  NameHasOwnerCallback callback);

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_NAME_HAS_OWNER_H_
