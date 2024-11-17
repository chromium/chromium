// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/check_for_service_and_start.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/dbus/utils/name_has_owner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace dbus_utils {

namespace {

constexpr char kMethodListActivatableNames[] = "ListActivatableNames";
constexpr char kMethodStartServiceByName[] = "StartServiceByName";

void OnStartServiceByNameResponse(scoped_refptr<dbus::Bus> bus,
                                  const std::string& name,
                                  CheckForServiceAndStartCallback callback,
                                  dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  std::optional<bool> started;
  uint32_t start_service_reply = 0;
  if (reader.PopUint32(&start_service_reply)) {
    started = start_service_reply == DBUS_START_REPLY_SUCCESS ||
              start_service_reply == DBUS_START_REPLY_ALREADY_RUNNING;
  } else {
    LOG(ERROR) << "Failed to read " << kMethodStartServiceByName << " response";
  }
  std::move(callback).Run(started);
}

void StartServiceByName(scoped_refptr<dbus::Bus> bus,
                        const std::string& name,
                        CheckForServiceAndStartCallback callback) {
  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kMethodStartServiceByName);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  // No flags
  writer.AppendUint32(0);
  proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                    base::BindOnce(&OnStartServiceByNameResponse, bus, name,
                                   std::move(callback)));
}

void OnListActivatableNamesResponse(scoped_refptr<dbus::Bus> bus,
                                    const std::string& name,
                                    CheckForServiceAndStartCallback callback,
                                    dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  std::vector<std::string> activatable_names;
  if (!reader.PopArrayOfStrings(&activatable_names)) {
    LOG(ERROR) << "Failed to read " << kMethodListActivatableNames
               << " response";
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (base::Contains(activatable_names, name)) {
    StartServiceByName(bus, name, std::move(callback));
  } else {
    // The service is not activatable
    std::move(callback).Run(false);
  }
}

void ListActivatableNames(scoped_refptr<dbus::Bus> bus,
                          const std::string& name,
                          CheckForServiceAndStartCallback callback) {
  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS,
                               kMethodListActivatableNames);
  proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                    base::BindOnce(&OnListActivatableNamesResponse, bus, name,
                                   std::move(callback)));
}

void OnNameHasOwnerResponse(scoped_refptr<dbus::Bus> bus,
                            const std::string& name,
                            CheckForServiceAndStartCallback callback,
                            std::optional<bool> name_has_owner) {
  if (name_has_owner.value_or(true)) {
    // Error communicating with bus or service already running.
    std::move(callback).Run(name_has_owner);
    return;
  }

  // Try auto-starting the service if it is activatable.
  ListActivatableNames(bus, name, std::move(callback));
}

}  // namespace

void CheckForServiceAndStart(scoped_refptr<dbus::Bus> bus,
                             const std::string& name,
                             CheckForServiceAndStartCallback callback) {
  // Check if the service is already running.
  NameHasOwner(
      bus.get(), name,
      base::BindOnce(&OnNameHasOwnerResponse, bus, name, std::move(callback)));
}

}  // namespace dbus_utils
