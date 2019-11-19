// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/appfuse/arc_appfuse_bridge.h"

#include <sys/epoll.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/dbus/arc_appfuse_provider_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcAppfuseBridge.
class ArcAppfuseBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAppfuseBridge,
          ArcAppfuseBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAppfuseBridgeFactory";

  static ArcAppfuseBridgeFactory* GetInstance() {
    return base::Singleton<ArcAppfuseBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcAppfuseBridgeFactory>;
  ArcAppfuseBridgeFactory() = default;
  ~ArcAppfuseBridgeFactory() override = default;
};

void RunWithScopedHandle(base::OnceCallback<void(mojo::ScopedHandle)> callback,
                         base::Optional<base::ScopedFD> fd) {
  if (!fd || !fd.value().is_valid()) {
    LOG(ERROR) << "Invalid FD: fd.has_value() = " << fd.has_value();
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd.value())));
  if (!wrapped_handle.is_valid()) {
    LOG(ERROR) << "Failed to wrap handle";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  std::move(callback).Run(std::move(wrapped_handle));
}

}  // namespace

// static
ArcAppfuseBridge* ArcAppfuseBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAppfuseBridgeFactory::GetForBrowserContext(context);
}

ArcAppfuseBridge::ArcAppfuseBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->appfuse()->SetHost(this);
}

ArcAppfuseBridge::~ArcAppfuseBridge() {
  arc_bridge_service_->appfuse()->SetHost(nullptr);
}

void ArcAppfuseBridge::Mount(uint32_t uid,
                             int32_t mount_id,
                             MountCallback callback) {
  // This is always safe because DBusThreadManager outlives ArcServiceLauncher.
  chromeos::DBusThreadManager::Get()->GetArcAppfuseProviderClient()->Mount(
      uid, mount_id, base::BindOnce(&RunWithScopedHandle, std::move(callback)));
}

void ArcAppfuseBridge::Unmount(uint32_t uid,
                               int32_t mount_id,
                               UnmountCallback callback) {
  chromeos::DBusThreadManager::Get()->GetArcAppfuseProviderClient()->Unmount(
      uid, mount_id, std::move(callback));
}

void ArcAppfuseBridge::OpenFile(uint32_t uid,
                                int32_t mount_id,
                                int32_t file_id,
                                int32_t flags,
                                OpenFileCallback callback) {
  chromeos::DBusThreadManager::Get()->GetArcAppfuseProviderClient()->OpenFile(
      uid, mount_id, file_id, flags,
      base::BindOnce(&RunWithScopedHandle, std::move(callback)));
}

}  // namespace arc
