// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/drive_file_provider.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

DriveFileProvider::DriveFileProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(drive_service_);
}

DriveFileProvider::~DriveFileProvider() = default;

void DriveFileProvider::Start(const base::string16& query) {
  // TODO(crbug.com/1154513): Query for Drive files.
}

ash::AppListSearchResultType DriveFileProvider::ResultType() {
  return ash::AppListSearchResultType::kDriveFile;
}

}  // namespace app_list
