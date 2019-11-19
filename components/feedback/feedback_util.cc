// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_util.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "third_party/zlib/google/zip.h"

namespace feedback_util {
namespace {

constexpr char kAtGoogleDotCom[] = "@google.com";

}  // namespace

bool ZipString(const base::FilePath& filename,
               const std::string& data, std::string* compressed_logs) {
  base::ScopedTempDir temp_dir;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!temp_dir.CreateUniqueTempDir())
    return false;
  if (base::WriteFile(temp_dir.GetPath().Append(filename), data.c_str(),
                      data.size()) == -1) {
    return false;
  }

  bool succeed = base::CreateTemporaryFile(&zip_file) &&
                 zip::Zip(temp_dir.GetPath(), zip_file, false) &&
                 base::ReadFileToString(zip_file, compressed_logs);

  base::DeleteFile(zip_file, false);

  return succeed;
}

bool IsGoogleEmail(const std::string& email) {
  return base::EndsWith(email, kAtGoogleDotCom,
                        base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace feedback_util
