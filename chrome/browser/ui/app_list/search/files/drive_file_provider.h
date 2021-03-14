// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_FILE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_FILE_PROVIDER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "components/drive/file_errors.h"

class Profile;

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace app_list {

class FileResult;

class DriveFileProvider : public SearchProvider {
 public:
  explicit DriveFileProvider(Profile* profile);
  ~DriveFileProvider() override;

  DriveFileProvider(const DriveFileProvider&) = delete;
  DriveFileProvider& operator=(const DriveFileProvider&) = delete;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() override;
  void Start(const std::u16string& query) override;

 private:
  void SetSearchResults(drive::FileError error,
                        std::vector<base::FilePath> paths);
  std::unique_ptr<FileResult> MakeResult(const base::FilePath& path);

  base::Optional<chromeos::string_matching::TokenizedString>
      last_tokenized_query_;

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;

  base::WeakPtrFactory<DriveFileProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_FILE_PROVIDER_H_
