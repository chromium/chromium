// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_FILE_SUGGESTER_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_FILE_SUGGESTER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "url/gurl.h"

class Profile;

class PickerFileSuggester {
 public:
  struct LocalFile {
    std::u16string title;
    base::FilePath path;
  };

  struct DriveFile {
    std::u16string title;
    base::FilePath local_path;
    GURL url;
  };

  using RecentLocalFilesCallback =
      base::OnceCallback<void(std::vector<LocalFile>)>;
  using RecentDriveFilesCallback =
      base::OnceCallback<void(std::vector<DriveFile>)>;

  explicit PickerFileSuggester(Profile* profile);
  ~PickerFileSuggester();
  PickerFileSuggester(const PickerFileSuggester&) = delete;
  PickerFileSuggester& operator=(const PickerFileSuggester&) = delete;

  // Any in-flight requests are cancelled when this object is destroyed.
  void GetRecentLocalFiles(RecentLocalFilesCallback callback);
  void GetRecentDriveFiles(RecentDriveFilesCallback callback);

 private:
  void OnGetRecentLocalFiles(RecentLocalFilesCallback callback,
                             const std::vector<ash::RecentFile>& recent_files);
  void OnGetRecentDriveFiles(RecentDriveFilesCallback callback,
                             const std::vector<ash::RecentFile>& recent_files);

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<PickerFileSuggester> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_FILE_SUGGESTER_H_
