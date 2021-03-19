// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_PROVIDER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/components/string_matching/tokenized_string.h"

class Profile;

namespace app_list {

class FileResult;

class LocalFileProvider : public SearchProvider {
 public:
  explicit LocalFileProvider(Profile* profile);
  ~LocalFileProvider() override;

  LocalFileProvider(const LocalFileProvider&) = delete;
  LocalFileProvider& operator=(const LocalFileProvider&) = delete;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() override;
  void Start(const std::u16string& query) override;

 private:
  void OnSearchComplete(const std::vector<base::FilePath>& paths);
  std::unique_ptr<FileResult> MakeResult(const base::FilePath& path);

  base::Optional<chromeos::string_matching::TokenizedString>
      last_tokenized_query_;

  Profile* const profile_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LocalFileProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_PROVIDER_H_
