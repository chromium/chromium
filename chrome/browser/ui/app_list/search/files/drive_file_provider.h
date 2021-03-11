// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_FILE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_FILE_PROVIDER_H_

#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace app_list {

class DriveFileProvider : public SearchProvider {
 public:
  explicit DriveFileProvider(Profile* profile);
  ~DriveFileProvider() override;

  DriveFileProvider(const DriveFileProvider&) = delete;
  DriveFileProvider& operator=(const DriveFileProvider&) = delete;

  // SearchProvider:
  void Start(const base::string16& query) override;
  ash::AppListSearchResultType ResultType() override;

 private:
  Profile* const profile_;

  drive::DriveIntegrationService* const drive_service_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_FILE_PROVIDER_H_
