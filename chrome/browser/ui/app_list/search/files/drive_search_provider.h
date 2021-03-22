// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "components/drive/file_errors.h"

class Profile;

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace app_list {

class FileResult;

class DriveSearchProvider : public SearchProvider {
 public:
  explicit DriveSearchProvider(Profile* profile);
  ~DriveSearchProvider() override;

  DriveSearchProvider(const DriveSearchProvider&) = delete;
  DriveSearchProvider& operator=(const DriveSearchProvider&) = delete;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() override;
  void Start(const std::u16string& query) override;

 private:
  void SetSearchResults(drive::FileError error,
                        std::vector<drivefs::mojom::QueryItemPtr> paths);
  std::unique_ptr<FileResult> MakeResult(const base::FilePath& path);

  base::TimeTicks query_start_time_;
  base::Optional<chromeos::string_matching::TokenizedString>
      last_tokenized_query_;

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveSearchProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_
