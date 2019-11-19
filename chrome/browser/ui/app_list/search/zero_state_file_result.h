// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_FILE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_FILE_RESULT_H_

#include <iosfwd>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "url/gurl.h"

class Profile;

namespace app_list {

// A search result representing a local file for zero state search results.
class ZeroStateFileResult : public ChromeSearchResult {
 public:
  ZeroStateFileResult(const base::FilePath& filepath,
                      float relevance,
                      Profile* profile);
  ~ZeroStateFileResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  ash::SearchResultType GetSearchResultType() const override;

 private:
  void Initialize();

  const base::FilePath filepath_;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ZeroStateFileResult);
};

::std::ostream& operator<<(::std::ostream& os,
                           const ZeroStateFileResult& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_FILE_RESULT_H_
