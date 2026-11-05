// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/camera_portal.h"

#include <map>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus_xdg {

namespace {

constexpr char kCameraInterface[] = "org.freedesktop.portal.Camera";
constexpr char kMethodAccessCamera[] = "AccessCamera";
constexpr char kMethodOpenPipeWireRemote[] = "OpenPipeWireRemote";
constexpr std::string_view kDesktopSuffix = ".desktop";

scoped_refptr<dbus::Bus> CreatePrivateSessionBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  return base::MakeRefCounted<dbus::Bus>(std::move(options));
}

// Returns the desktop file name without the ".desktop" suffix, e.g.
// "google-chrome".
std::string GetDesktopAppId() {
  auto env = base::Environment::Create();
  std::string name = version_info::nix::GetDesktopName(*env);
  if (name.ends_with(kDesktopSuffix)) {
    name.resize(name.size() - kDesktopSuffix.size());
  }
  return name;
}

}  // namespace

CameraPortal::CameraPortal() : CameraPortal(CreatePrivateSessionBus()) {}

CameraPortal::CameraPortal(scoped_refptr<dbus::Bus> bus)
    : bus_(std::move(bus)) {}

CameraPortal::~CameraPortal() {
  // Close the request before the bus shuts down.
  access_camera_request_.reset();
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
}

void CameraPortal::OpenPipeWireRemote(CameraPortalCallback callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);

  const std::string app_id = GetDesktopAppId();
  if (base::nix::GetAppSandbox() != base::nix::AppSandbox::kNone ||
      app_id.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraPortal::Finish, weak_ptr_factory_.GetWeakPtr(),
                       base::unexpected(CameraPortalError::kFailed)));
    return;
  }

  // Register must be the first portal call on this connection.
  dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
  dbus_utils::CallMethod<"sa{sv}", "">(
      proxy, kRegistryInterface, kMethodRegister,
      base::BindOnce(&CameraPortal::OnRegisterResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      app_id, std::map<std::string, dbus_utils::Variant>());
}

void CameraPortal::OnRegisterResponse(dbus_utils::CallMethodResult<> result) {
  if (!result.has_value()) {
    LOG(WARNING) << "Failed to register with " << kRegistryInterface;
    Finish(base::unexpected(CameraPortalError::kFailed));
    return;
  }

  dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
  access_camera_request_ = std::make_unique<Request>(
      bus_, proxy, kCameraInterface, kMethodAccessCamera, Dictionary(),
      base::BindOnce(&CameraPortal::OnAccessCameraResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CameraPortal::OnAccessCameraResponse(Results results) {
  if (!results.has_value()) {
    Finish(base::unexpected(results.error() ==
                                    ResponseError::kRequestCancelledByUser
                                ? CameraPortalError::kDenied
                                : CameraPortalError::kFailed));
    return;
  }

  dbus::ObjectProxy* proxy = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
  dbus_utils::CallMethod<"a{sv}", "h">(
      proxy, kCameraInterface, kMethodOpenPipeWireRemote,
      base::BindOnce(&CameraPortal::OnOpenPipeWireRemoteResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      std::map<std::string, dbus_utils::Variant>());
}

void CameraPortal::OnOpenPipeWireRemoteResponse(
    dbus_utils::CallMethodResult<base::ScopedFD> result) {
  if (!result.has_value() || !std::get<0>(*result).is_valid()) {
    LOG(ERROR) << "Failed to open the PipeWire remote for the cameras.";
    Finish(base::unexpected(CameraPortalError::kFailed));
    return;
  }
  Finish(std::move(std::get<0>(*result)));
}

void CameraPortal::Finish(CameraPortalResult result) {
  std::move(callback_).Run(std::move(result));
  // `this` may be destroyed by the callback.
}

}  // namespace dbus_xdg
