// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/import/password_csv_reader.h"

namespace password_manager {

namespace {

// Preferred filename extension for the imported files.
const base::FilePath::CharType kFileExtension[] = FILE_PATH_LITERAL("csv");

// Reads and returns the contents of the file at |path| as a string, or returns
// a null value on error.
base::Optional<std::string> ReadFileToString(const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return base::Optional<std::string>();
  return contents;
}

// Parses passwords from |input| using |password_reader| and synchronously calls
// |completion| with the results.
static void ParsePasswords(
    const PasswordImporter::CompletionCallback& completion,
    base::Optional<std::string> input) {
  // Currently, CSV is the only supported format.
  PasswordCSVReader password_reader;
  std::vector<autofill::PasswordForm> passwords;
  PasswordImporter::Result result = PasswordImporter::IO_ERROR;
  if (input)
    result = password_reader.DeserializePasswords(input.value(), &passwords);
  completion.Run(result, passwords);
}

}  // namespace

// static
void PasswordImporter::Import(const base::FilePath& path,
                              const CompletionCallback& completion) {
  // Posting with USER_VISIBLE priority, because the result of the import is
  // visible to the user in the password settings page.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::Bind(&ReadFileToString, path),
      base::Bind(&ParsePasswords, completion));
}

// static
std::vector<std::vector<base::FilePath::StringType>>
PasswordImporter::GetSupportedFileExtensions() {
  return std::vector<std::vector<base::FilePath::StringType>>(
      1, std::vector<base::FilePath::StringType>(1, kFileExtension));
}

}  // namespace password_manager
