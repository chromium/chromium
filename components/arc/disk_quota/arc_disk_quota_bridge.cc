// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/disk_quota/arc_disk_quota_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {

namespace {

// Singleton factory for ArcDiskQuotaBridge.
class ArcDiskQuotaBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcDiskQuotaBridge,
          ArcDiskQuotaBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcDiskQuotaBridgeFactory";

  static ArcDiskQuotaBridgeFactory* GetInstance() {
    return base::Singleton<ArcDiskQuotaBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcDiskQuotaBridgeFactory>;
  ArcDiskQuotaBridgeFactory() = default;
  ~ArcDiskQuotaBridgeFactory() override = default;
};

}  // namespace

// static
ArcDiskQuotaBridge* ArcDiskQuotaBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDiskQuotaBridgeFactory::GetForBrowserContext(context);
}

ArcDiskQuotaBridge::ArcDiskQuotaBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->disk_quota()->SetHost(this);
}

ArcDiskQuotaBridge::~ArcDiskQuotaBridge() {
  arc_bridge_service_->disk_quota()->SetHost(nullptr);
}

void ArcDiskQuotaBridge::IsQuotaSupported(IsQuotaSupportedCallback callback) {
  chromeos::CryptohomeClient::Get()->IsQuotaSupported(base::BindOnce(
      [](IsQuotaSupportedCallback callback, base::Optional<bool> result) {
        LOG_IF(ERROR, !result.has_value())
            << "Failed to retrieve result from IsQuotaSupported call.";
        std::move(callback).Run(result.value_or(false));
      },
      std::move(callback)));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForUid(
    uint32_t uid,
    GetCurrentSpaceForUidCallback callback) {
  chromeos::CryptohomeClient::Get()->GetCurrentSpaceForUid(
      uid, base::BindOnce(
               [](GetCurrentSpaceForUidCallback callback, int uid,
                  base::Optional<int64_t> result) {
                 LOG_IF(ERROR, !result.has_value())
                     << "Failed to retrieve result from "
                        "GetCurrentSpaceForUid for android uid="
                     << uid;
                 std::move(callback).Run(result.value_or(-1LL));
               },
               std::move(callback), uid));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForGid(
    uint32_t gid,
    GetCurrentSpaceForGidCallback callback) {
  chromeos::CryptohomeClient::Get()->GetCurrentSpaceForGid(
      gid, base::BindOnce(
               [](GetCurrentSpaceForGidCallback callback, int gid,
                  base::Optional<int64_t> result) {
                 LOG_IF(ERROR, !result.has_value())
                     << "Failed to retrieve result from "
                        "GetCurrentSpaceForGid for android gid="
                     << gid;
                 std::move(callback).Run(result.value_or(-1LL));
               },
               std::move(callback), gid));
}

}  // namespace arc
