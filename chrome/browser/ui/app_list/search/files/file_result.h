// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_RESULT_H_

#include <iosfwd>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class SkBitmap;

namespace ash {
class ThumbnailLoader;
}

namespace app_list {

class FileResult : public ChromeSearchResult {
 public:
  enum class Type { kFile, kDirectory, kSharedDirectory };

  FileResult(const std::string& schema,
             const base::FilePath& filepath,
             ResultType result_type,
             DisplayType display_type,
             float relevance,
             const std::u16string& query,
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

  // Depending on the file type and display type, request a thumbnail for this
  // result. If the request is successful, the current icon will be replaced by
  // the thumbnail.
  void RequestThumbnail(ash::ThumbnailLoader* thumbnail_loader);

 private:
  // Callback for the result of MaybeRequestThumbnail's call to the
  // ThumbnailLoader.
  void OnThumbnailLoaded(const SkBitmap* bitmap, base::File::Error error);

  const base::FilePath filepath_;
  const Type type_;
  Profile* const profile_;

  base::WeakPtrFactory<FileResult> weak_factory_{this};
};

::std::ostream& operator<<(::std::ostream& os, const FileResult& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_RESULT_H_
