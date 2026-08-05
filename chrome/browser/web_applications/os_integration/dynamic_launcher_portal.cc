// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/dynamic_launcher_portal.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace web_app {

namespace {

// Cached availability result when using the shared session bus.
// std::nullopt: Query in progress or uninitialized.
// true: Portal is available and webapp launcher type is supported.
// false: Portal is unavailable or webapp launcher type is unsupported.
std::optional<bool> g_cached_availability;

std::vector<base::OnceCallback<void(bool)>>& GetPendingCallbacks() {
  static base::NoDestructor<std::vector<base::OnceCallback<void(bool)>>>
      callbacks;
  return *callbacks;
}

// Value 2u corresponds to LauncherType::WebApp in the
// org.freedesktop.portal.DynamicLauncher XDG portal spec.
constexpr uint32_t kLauncherTypeWebapp = 2u;

constexpr char kSupportedLauncherTypesProperty[] = "SupportedLauncherTypes";
constexpr char kVersionProperty[] = "version";

void CompleteAvailabilityCheck(bool is_custom_bus,
                               base::OnceCallback<void(bool)> callback,
                               std::optional<bool> cached_value,
                               bool result_for_callbacks) {
  if (!is_custom_bus) {
    g_cached_availability = cached_value;
    auto pending = std::move(GetPendingCallbacks());
    GetPendingCallbacks().clear();
    for (auto& cb : pending) {
      std::move(cb).Run(result_for_callbacks);
    }
  } else {
    std::move(callback).Run(result_for_callbacks);
  }
}

// Callback for the PrepareInstall D-Bus portal request.
void OnPrepareInstallResponse(
    std::unique_ptr<dbus_xdg::Request> request,
    DynamicLauncherPortal::PrepareInstallCallback callback,
    dbus_xdg::Results results) {
  if (!results.has_value()) {
    LOG(ERROR) << "DynamicLauncher portal PrepareInstall failed or was denied";
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto it = results->find("token");
  if (it == results->end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<std::string> token = std::move(it->second).Take<std::string>();
  std::move(callback).Run(token);
}

// Callback for querying the SupportedLauncherTypes property of the portal.
void OnSupportedLauncherTypesResponse(
    bool is_custom_bus,
    base::OnceCallback<void(bool)> callback,
    dbus_utils::CallMethodResultSig<"v"> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "DynamicLauncher portal SupportedLauncherTypes property "
                  "query failed";
    CompleteAvailabilityCheck(is_custom_bus, std::move(callback),
                              /*cached_value=*/std::nullopt,
                              /*result_for_callbacks=*/false);
    return;
  }
  dbus_utils::Variant variant = std::get<0>(std::move(*result));
  std::optional<uint32_t> supported_types = std::move(variant).Take<uint32_t>();
  if (!supported_types.has_value()) {
    CompleteAvailabilityCheck(is_custom_bus, std::move(callback),
                              /*cached_value=*/false,
                              /*result_for_callbacks=*/false);
    return;
  }
  bool available = (*supported_types & kLauncherTypeWebapp) != 0;
  CompleteAvailabilityCheck(is_custom_bus, std::move(callback),
                            /*cached_value=*/available,
                            /*result_for_callbacks=*/available);
}

// Callback for querying the portal interface version property.
void OnPortalVersionResponse(scoped_refptr<dbus::Bus> bus,
                             bool is_custom_bus,
                             base::OnceCallback<void(bool)> callback,
                             dbus_utils::CallMethodResultSig<"v"> result) {
  if (!result.has_value()) {
    CompleteAvailabilityCheck(is_custom_bus, std::move(callback),
                              /*cached_value=*/false,
                              /*result_for_callbacks=*/false);
    return;
  }
  std::optional<uint32_t> version =
      std::get<0>(std::move(*result)).Take<uint32_t>();
  if (!version.has_value() || *version == 0) {
    CompleteAvailabilityCheck(is_custom_bus, std::move(callback),
                              /*cached_value=*/false,
                              /*result_for_callbacks=*/false);
    return;
  }

  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(dbus_xdg::kPortalServiceName,
                          dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
  // Call org.freedesktop.DBus.Properties.Get.
  // "ss" signature: string interface_name, string property_name.
  // "v" signature: returns a Variant containing the property value.
  dbus_utils::CallMethod<"ss", "v">(
      proxy, DBUS_INTERFACE_PROPERTIES, "Get",
      base::BindOnce(&OnSupportedLauncherTypesResponse, is_custom_bus,
                     std::move(callback)),
      std::string(kDynamicLauncherInterfaceName),
      std::string(kSupportedLauncherTypesProperty));
}

// Callback for checking if the overall XdgDesktopPortal service is available.
void OnXdgDesktopPortalResponse(scoped_refptr<dbus::Bus> bus,
                                bool is_custom_bus,
                                base::OnceCallback<void(bool)> callback,
                                uint32_t portal_version) {
  if (portal_version == 0) {
    CompleteAvailabilityCheck(is_custom_bus, std::move(callback),
                              /*cached_value=*/false,
                              /*result_for_callbacks=*/false);
    return;
  }
  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(dbus_xdg::kPortalServiceName,
                          dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
  // Call org.freedesktop.DBus.Properties.Get.
  // "ss" signature: string interface_name, string property_name.
  // "v" signature: returns a Variant containing the property value.
  dbus_utils::CallMethod<"ss", "v">(
      proxy, DBUS_INTERFACE_PROPERTIES, "Get",
      base::BindOnce(&OnPortalVersionResponse, bus, is_custom_bus,
                     std::move(callback)),
      std::string(kDynamicLauncherInterfaceName),
      std::string(kVersionProperty));
}

}  // namespace

DynamicLauncherPortal::DynamicLauncherPortal(scoped_refptr<dbus::Bus> bus)
    : bus_(bus ? std::move(bus) : dbus_thread_linux::GetSharedSessionBus()) {}

DynamicLauncherPortal::~DynamicLauncherPortal() = default;

void DynamicLauncherPortal::IsAvailable(
    base::OnceCallback<void(bool)> callback) {
  if (!bus_) {
    std::move(callback).Run(false);
    return;
  }

  bool is_custom_bus = (bus_ != dbus_thread_linux::GetSharedSessionBus());

  if (!is_custom_bus) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  if (!is_custom_bus && g_cached_availability.has_value()) {
    std::move(callback).Run(*g_cached_availability);
    return;
  }

  if (!is_custom_bus) {
    GetPendingCallbacks().push_back(std::move(callback));
    if (GetPendingCallbacks().size() > 1) {
      // Query already in flight on the shared bus.
      return;
    }
  }

  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(),
      base::BindOnce(&OnXdgDesktopPortalResponse, bus_, is_custom_bus,
                     is_custom_bus ? std::move(callback) : base::DoNothing()));
}

void DynamicLauncherPortal::PrepareInstall(
    const std::string& name,
    const std::vector<uint8_t>& icon_bytes,
    const GURL& target_url,
    PrepareInstallCallback callback) {
  if (bus_ == dbus_thread_linux::GetSharedSessionBus()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  if (icon_bytes.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!bus_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  dbus::ObjectProxy* proxy =
      bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                           dbus::ObjectPath(dbus_xdg::kPortalObjectPath));

  dbus_xdg::Dictionary options;
  if (target_url.is_valid()) {
    options["target"] = dbus_utils::Variant::Wrap<"s">(target_url.spec());
  }
  options["launcher_type"] =
      dbus_utils::Variant::Wrap<"u">(kLauncherTypeWebapp);
  options["editable_name"] = dbus_utils::Variant::Wrap<"b">(false);

  // Construct GBytesIcon variant (sv): ("bytes", v(ay))
  dbus_utils::Variant inner_variant =
      dbus_utils::Variant::Wrap<"ay">(icon_bytes);
  auto bytes_icon_tuple =
      std::make_tuple(std::string("bytes"), std::move(inner_variant));
  dbus_utils::Variant icon_variant =
      dbus_utils::Variant::Wrap<"(sv)">(std::move(bytes_icon_tuple));

  auto request = std::make_unique<dbus_xdg::Request>(
      bus_.get(), proxy, kDynamicLauncherInterfaceName, "PrepareInstall",
      std::move(options), /*parent_window=*/std::string(), name, icon_variant);
  auto* request_ptr = request.get();
  request_ptr->SetCallback(base::BindOnce(
      &OnPrepareInstallResponse, std::move(request), std::move(callback)));
}

void DynamicLauncherPortal::Install(const std::string& token,
                                    const std::string& desktop_file_id,
                                    const std::string& desktop_entry,
                                    InstallCallback callback) {
  if (bus_ == dbus_thread_linux::GetSharedSessionBus()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  if (!bus_) {
    std::move(callback).Run(false);
    return;
  }

  dbus::ObjectProxy* proxy =
      bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                           dbus::ObjectPath(dbus_xdg::kPortalObjectPath));

  dbus_xdg::Dictionary options;
  dbus_utils::CallMethod<"sssa{sv}", "">(
      proxy, kDynamicLauncherInterfaceName, "Install",
      base::BindOnce(
          [](InstallCallback callback, dbus_utils::CallMethodResult<> result) {
            std::move(callback).Run(result.has_value());
          },
          std::move(callback)),
      token, desktop_file_id, desktop_entry, options);
}

// static
void DynamicLauncherPortal::ResetAvailabilityCacheForTesting() {
  g_cached_availability.reset();
  GetPendingCallbacks().clear();
}

void DynamicLauncherPortal::Uninstall(const std::string& desktop_file_id,
                                      UninstallCallback callback) {
  if (bus_ == dbus_thread_linux::GetSharedSessionBus()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  if (!bus_) {
    std::move(callback).Run(false);
    return;
  }

  dbus::ObjectProxy* proxy =
      bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                           dbus::ObjectPath(dbus_xdg::kPortalObjectPath));

  dbus_xdg::Dictionary options;
  dbus_utils::CallMethod<"sa{sv}", "">(
      proxy, kDynamicLauncherInterfaceName, "Uninstall",
      base::BindOnce(
          [](UninstallCallback callback,
             dbus_utils::CallMethodResult<> result) {
            std::move(callback).Run(result.has_value());
          },
          std::move(callback)),
      desktop_file_id, options);
}

}  // namespace web_app
