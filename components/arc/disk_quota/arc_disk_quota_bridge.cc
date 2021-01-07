// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/disk_quota/arc_disk_quota_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
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

constexpr char kAndroidDownloadPath[] = "/storage/emulated/0/Download/";
constexpr char kAndroidExternalStoragePath[] = "/storage/emulated/0/";
constexpr char kAndroidDataMediaPath[] = "/data/media/0/";

}  // namespace

// static
bool ArcDiskQuotaBridge::convertPathForSetProjectId(
    const base::FilePath& android_path,
    cryptohome::SetProjectIdAllowedPathType* parent_path_out,
    base::FilePath* child_path_out) {
  const base::FilePath kDownloadPath(kAndroidDownloadPath);
  const base::FilePath kExternalStoragePath(kAndroidExternalStoragePath);
  const base::FilePath kDataMediaPath(kAndroidDataMediaPath);

  if (android_path.ReferencesParent()) {
    LOG(ERROR) << "Path contains \"..\" : " << android_path.value();
    return false;
  }

  *child_path_out = base::FilePath();
  if (kDownloadPath.IsParent(android_path)) {
    // /storage/emulated/0/Download/* =>
    //     parent=/home/user/<hash>/Downloads/, child=*
    *parent_path_out = cryptohome::SetProjectIdAllowedPathType::PATH_DOWNLOADS;
    return kDownloadPath.AppendRelativePath(android_path, child_path_out);
  } else if (kExternalStoragePath.IsParent(android_path)) {
    // /storage/emulated/0/* =>
    //     parent=/home/root/<hash>/android-data/, child=data/media/0/*
    *parent_path_out =
        cryptohome::SetProjectIdAllowedPathType::PATH_ANDROID_DATA;
    // child_path should be relative to the root.
    return base::FilePath("/").AppendRelativePath(kDataMediaPath,
                                                  child_path_out) &&
           kExternalStoragePath.AppendRelativePath(android_path,
                                                   child_path_out);
  } else if (kDataMediaPath.IsParent(android_path)) {
    // /data/media/0/* =>
    //     parent=/home/root/<hash>/android-data/, child=data/media/0/*
    *parent_path_out =
        cryptohome::SetProjectIdAllowedPathType::PATH_ANDROID_DATA;
    // child_path should be relative to the root.
    return base::FilePath("/").AppendRelativePath(android_path, child_path_out);
  } else {
    return false;
  }
}

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

void ArcDiskQuotaBridge::SetAccountId(const AccountId& account_id) {
  account_id_ = account_id;
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

void ArcDiskQuotaBridge::GetCurrentSpaceForProjectId(
    uint32_t project_id,
    GetCurrentSpaceForProjectIdCallback callback) {
  chromeos::CryptohomeClient::Get()->GetCurrentSpaceForProjectId(
      project_id, base::BindOnce(
                      [](GetCurrentSpaceForProjectIdCallback callback,
                         int project_id, base::Optional<int64_t> result) {
                        LOG_IF(ERROR, !result.has_value())
                            << "Failed to retrieve result from "
                               "GetCurrentSpaceForProjectId for project_id="
                            << project_id;
                        std::move(callback).Run(result.value_or(-1LL));
                      },
                      std::move(callback), project_id));
}

void ArcDiskQuotaBridge::SetProjectId(uint32_t project_id,
                                      const std::string& android_path,
                                      SetProjectIdCallback callback) {
  cryptohome::SetProjectIdAllowedPathType parent_path;
  base::FilePath child_path;
  if (!convertPathForSetProjectId(base::FilePath(android_path), &parent_path,
                                  &child_path)) {
    LOG(ERROR) << "Setting a project ID to path " << android_path
               << " is not allowed";
    std::move(callback).Run(false);
    return;
  }

  auto identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_);

  chromeos::CryptohomeClient::Get()->SetProjectId(
      project_id, parent_path, child_path.value(), identifier,
      base::BindOnce(
          [](SetProjectIdCallback callback, const int project_id,
             const cryptohome::SetProjectIdAllowedPathType parent_path,
             const std::string& child_path,
             const cryptohome::AccountIdentifier& account_id,
             base::Optional<bool> result) {
            LOG_IF(ERROR, !result.has_value())
                << "Failed to set project ID " << project_id
                << " to parent_path=" << parent_path
                << " child_path=" << child_path;
            std::move(callback).Run(result.value_or(false));
          },
          std::move(callback), project_id, parent_path, child_path.value(),
          identifier));
}

}  // namespace arc
