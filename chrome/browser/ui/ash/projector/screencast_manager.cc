// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <vector>

#include "ash/webui/projector_app/projector_screencast.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/drive/service/drive_api_service.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Projector network traffic annotation tags.
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("projector_drive_request",
                                        R"(
          semantics: {
            sender: "ChromeOS Projector"
            description: "ChromeOS send Drive request for Projector"
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
          })");

// Populates the `metadata_file_id` and `video.file_id` fields for `screencast`
// on get the results from GetFileListInDirectory().
void OnGetScreencastFilesResult(
    std::unique_ptr<ProjectorScreencast> screencast,
    ProjectorAppClient::OnGetScreencastCallback callback,
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::FileList> entry) {
  // Copies screencast id since `screencast` will be deleted after std::move().
  const std::string screencast_id = screencast->container_folder_id;
  if (error != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    std::move(callback).Run(
        std::move(screencast),
        base::StringPrintf(
            "Search Drive files error. ScreencastId=%s, ErrorCode=%d",
            screencast_id.c_str(), error));
    return;
  }

  for (const auto& file_resource : entry->items()) {
    if (base::EndsWith(file_resource->title(), kProjectorMediaFileExtension)) {
      screencast->video.file_id = file_resource->file_id();
    } else if (base::EndsWith(file_resource->title(),
                              kProjectorMetadataFileExtension)) {
      screencast->metadata_file_id = file_resource->file_id();
    }
  }

  std::string error_msg;
  if (screencast->metadata_file_id.empty() ||
      screencast->video.file_id.empty()) {
    error_msg = base::StringPrintf(
        "Invalid screencast. Missing video file or metadata file. "
        "ScreencastId=%s",
        screencast_id.c_str());
  }
  std::move(callback).Run(std::move(screencast), error_msg);
}

}  // namespace

ScreencastManager::ScreencastManager() = default;
ScreencastManager::~ScreencastManager() = default;

// TODO(b/237089852): Find the path for local video file.
// TODO(b/236857019): Set the rest of the fields for the screencast.
void ScreencastManager::GetScreencast(
    const std::string& screencast_id,
    ProjectorAppClient::OnGetScreencastCallback callback) {
  DCHECK(!screencast_id.empty());
  std::unique_ptr<ProjectorScreencast> screencast =
      std::make_unique<ProjectorScreencast>();
  screencast->container_folder_id = screencast_id;
  if (!drive_api_service_)
    InitDriveAPIService();

  drive_api_service_->GetFileListInDirectory(
      screencast_id,
      base::BindOnce(&OnGetScreencastFilesResult, std::move(screencast),
                     std::move(callback)));
}

void ScreencastManager::SetDriveAPIServiceForTest(
    std::unique_ptr<drive::DriveServiceInterface> drive_api_service) {
  drive_api_service_ = std::move(drive_api_service);
}

void ScreencastManager::InitDriveAPIService() {
  // TODO(b/221492092): Add one DriveFS/Drive helper to manage the profile
  // switch.
  signin::IdentityManager* identity_manager =
      ProjectorAppClient::Get()->GetIdentityManager();
  auto* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  DCHECK(identity_manager);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  GURL base_url(GaiaUrls::GetInstance()->google_apis_origin_url());
  GURL base_thumbnail_url(
      google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction);
  drive_api_service_ = std::make_unique<drive::DriveAPIService>(
      identity_manager, url_loader_factory, blocking_task_runner.get(),
      base_url, base_thumbnail_url, /*custom_user_agent=*/std::string(),
      kNetworkTrafficAnnotationTag);

  // The screencast object will be used to load screencast from DriveFS and
  // since DriveFS only support primary account, we don't need to handle
  // secondary account here.
  drive_api_service_->Initialize(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
}

}  // namespace ash
