// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_PROVIDER_H_

#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

class LocalFileProvider : public SearchProvider {
 public:
  explicit LocalFileProvider(Profile* profile);
  ~LocalFileProvider() override;

  LocalFileProvider(const LocalFileProvider&) = delete;
  LocalFileProvider& operator=(const LocalFileProvider&) = delete;

  // SearchProvider:
  void Start(const base::string16& query) override;
  ash::AppListSearchResultType ResultType() override;

 private:
  Profile* const profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_LOCAL_FILE_PROVIDER_H_
