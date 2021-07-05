// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_RESULT_H_

#include <iosfwd>

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace app_list {

class FileResult : public ChromeSearchResult {
 public:
  enum class Type { kFile, kDirectory, kSharedDirectory };

  // Constructor for zero state results.
  FileResult(const std::string& schema,
             const base::FilePath& filepath,
             ResultType result_type,
             DisplayType display_type,
             float relevance,
             Profile* profile);
  // Constructor for search results.
  FileResult(const std::string& schema,
             const base::FilePath& filepath,
             ResultType result_type,
             const std::u16string& query,
             float relevance,
             Type type,
             Profile* profile);
  ~FileResult() override;

  FileResult(const FileResult&) = delete;
  FileResult& operator=(const FileResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

  // Calculates file's match relevance score. Will return a default score if the
  // query is missing or the filename is empty.
  static double CalculateRelevance(
      const absl::optional<chromeos::string_matching::TokenizedString>& query,
      const base::FilePath& filepath);

 private:
  FileResult(const std::string& schema,
             const base::FilePath& filepath,
             ResultType result_type,
             DisplayType display_type,
             Type type,
             Profile* profile);

  const base::FilePath filepath_;
  const Type type_;
  Profile* const profile_;
};

::std::ostream& operator<<(::std::ostream& os, const FileResult& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_RESULT_H_
