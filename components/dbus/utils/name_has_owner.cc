// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/name_has_owner.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus_utils {

namespace {

constexpr char kMethodNameHasOwner[] = "NameHasOwner";

void OnNameHasOwnerResponse(NameHasOwnerCallback callback,
                            dbus::Response* response) {
  std::optional<bool> result;
  if (response) {
    dbus::MessageReader reader(response);
    bool name_has_owner = false;
    if (reader.PopBool(&name_has_owner)) {
      result = name_has_owner;
    } else {
      LOG(ERROR) << "Failed to read " << kMethodNameHasOwner << " response";
    }
  }
  std::move(callback).Run(result);
}

}  // namespace

void NameHasOwner(dbus::Bus* bus,
                  const std::string& name,
                  NameHasOwnerCallback callback) {
  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kMethodNameHasOwner);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);

  proxy->CallMethod(
      &method_call, DBUS_TIMEOUT_USE_DEFAULT,
      base::BindOnce(OnNameHasOwnerResponse, std::move(callback)));
}

}  // namespace dbus_utils
