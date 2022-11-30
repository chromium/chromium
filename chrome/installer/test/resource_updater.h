// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper class for updating resources in portable executable files.

#ifndef CHROME_INSTALLER_TEST_RESOURCE_UPDATER_H_
#define CHROME_INSTALLER_TEST_RESOURCE_UPDATER_H_

#include <windows.h>

#include <string>
#include <utility>

namespace base {
class FilePath;
}

namespace upgrade_test {

// Updates resources in a PE image file.
class ResourceUpdater {
 public:
  ResourceUpdater();

  ResourceUpdater(const ResourceUpdater&) = delete;
  ResourceUpdater& operator=(const ResourceUpdater&) = delete;

  ~ResourceUpdater();

  // Loads |pe_image_path| in preparation for updating its resources.
  bool Initialize(const base::FilePath& pe_image_path);

  // Replaces the contents of the resource |name| of |type| and |language_id|
  // with the contents of |input_file|, returning true on success.
  bool Update(const std::wstring& name,
              const std::wstring& type,
              WORD language_id,
              const base::FilePath& input_file);

  // Commits all updates to the file on disk.
  bool Commit();

 private:
  HANDLE handle_;
};  // class ResourceUpdater

}  // namespace upgrade_test

#endif  // CHROME_INSTALLER_TEST_RESOURCE_UPDATER_H_
