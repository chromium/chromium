// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"

namespace password_manager {

class CSVPasswordSequence;

// Static-only class bundling together the API for importing passwords from a
// file.
class PasswordImporter {
 public:
  enum Result {
    SUCCESS,
    IO_ERROR,
    SYNTAX_ERROR,
    SEMANTIC_ERROR,
    NUM_IMPORT_RESULTS
  };

  // CompletionCallback is the type of the processing function for parsed
  // passwords.
  using CompletionCallback =
      base::OnceCallback<void(Result, CSVPasswordSequence)>;

  PasswordImporter() = delete;
  PasswordImporter(const PasswordImporter&) = delete;
  PasswordImporter& operator=(const PasswordImporter&) = delete;

  // Imports passwords from the file at |path|, and fires |completion| callback
  // on the calling thread with the passwords when ready. The only supported
  // file format is CSV.
  static void Import(const base::FilePath& path, CompletionCallback completion);

  // Returns the file extensions corresponding to supported formats.
  static std::vector<std::vector<base::FilePath::StringType>>
  GetSupportedFileExtensions();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
