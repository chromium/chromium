// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

namespace {

// Preferred filename extension for the imported files.
const base::FilePath::CharType kFileExtension[] = FILE_PATH_LITERAL("csv");

// Reads and returns the contents of the file at |path| as a string, or returns
// a null value on error.
absl::optional<std::string> ReadFileToString(const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return absl::optional<std::string>();
  return contents;
}

PasswordImporter::Result ToImporterError(CSVPassword::Status status) {
  switch (status) {
    case CSVPassword::Status::kOK:
      return PasswordImporter::SUCCESS;
    case CSVPassword::Status::kSyntaxError:
      return PasswordImporter::SYNTAX_ERROR;
    case CSVPassword::Status::kSemanticError:
      return PasswordImporter::SEMANTIC_ERROR;
  }
}

// Parses passwords from |input| using |password_reader| and synchronously calls
// |completion| with the results.
static void ParsePasswords(PasswordImporter::CompletionCallback completion,
                           absl::optional<std::string> input) {
  // Currently, CSV is the only supported format.
  if (!input) {
    std::move(completion)
        .Run(PasswordImporter::IO_ERROR, CSVPasswordSequence(std::string()));
    return;
  }
  CSVPasswordSequence seq(std::move(input.value()));
  PasswordImporter::Result result = ToImporterError(seq.result());
  std::move(completion).Run(result, std::move(seq));
}

}  // namespace

// static
void PasswordImporter::Import(const base::FilePath& path,
                              CompletionCallback completion) {
  // Posting with USER_VISIBLE priority, because the result of the import is
  // visible to the user in the password settings page.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadFileToString, path),
      base::BindOnce(&ParsePasswords, std::move(completion)));
}

// static
std::vector<std::vector<base::FilePath::StringType>>
PasswordImporter::GetSupportedFileExtensions() {
  return std::vector<std::vector<base::FilePath::StringType>>(
      1, std::vector<base::FilePath::StringType>(1, kFileExtension));
}

}  // namespace password_manager
