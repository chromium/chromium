// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"
#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"

namespace password_manager {

// Exposes an API for importing passwords from a file. Parsing of CSV will be
// performed using a utility SandBox process.
class PasswordImporter {
 public:
  enum Status {
    NONE,
    SUCCESS,
    IO_ERROR,
    SYNTAX_ERROR,
    SEMANTIC_ERROR,
    LARGE_FILE,
    MAX_STATUS
  };

  // CompletionCallback is the type of the processing function for parsed
  // passwords.
  using CompletionCallback =
      password_manager::mojom::CSVPasswordParser::ParseCSVCallback;

  PasswordImporter();
  PasswordImporter(const PasswordImporter&) = delete;
  PasswordImporter& operator=(const PasswordImporter&) = delete;
  ~PasswordImporter();

  // Imports passwords from the file at |path|, and fires |completion| callback
  // on the calling thread with the passwords when ready. The only supported
  // file format is CSV.
  void Import(const base::FilePath& path, CompletionCallback completion);

  // Returns the file extensions corresponding to supported formats.
  static std::vector<std::vector<base::FilePath::StringType>>
  GetSupportedFileExtensions();

  // Overrides the csv password parser service for testing.
  void SetServiceForTesting(
      mojo::PendingRemote<mojom::CSVPasswordParser> parser);

  // Returns the import status.
  Status GetStatus() const;

 private:
  // Parses passwords from |input| using a mojo sandbox process and
  // asynchronously calls |completion| with the results.
  void ParseCSVPasswordsInSandbox(
      CompletionCallback completion,
      base::expected<std::string, PasswordImporter::Status> result);

  void ConsumePasswords(password_manager::mojom::CSVPasswordSequencePtr seq);

  const mojo::Remote<mojom::CSVPasswordParser>& GetParser();

  mojo::Remote<mojom::CSVPasswordParser> parser_;

  Status status_{Status::NONE};

  base::WeakPtrFactory<PasswordImporter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
