// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/trash_service_impl.h"

#include <limits.h>

#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace ash::trash_service {

namespace {

// Represents the expected token on the first line of the .trashinfo file.
constexpr std::string_view kTrashInfoHeaderToken = "[Trash Info]";

// Represents the expected token starting the second line of the .trashinfo
// file.
constexpr std::string_view kPathToken = "Path=";

// Represents the expected token starting the third line of the .trashinfo file.
constexpr std::string_view kDeletionDateToken = "DeletionDate=";

// The "DeletionDate=" line contains 24 bytes representing a well formed
// ISO-8601 date string, e.g. "2022-07-18T10:13:00.000Z".
constexpr size_t kISO8601Size = 24;

// Helper function to invoke the supplied callback with an error and empty
// restore path and deletion date.
void InvokeCallbackWithError(
    base::File::Error error,
    TrashServiceImpl::ParseTrashInfoFileCallback callback) {
  std::move(callback).Run(error, base::FilePath(), base::Time());
}

// Helper function to return `base::File::FILE_ERROR_FAILED` to the supplied
// callback.
void InvokeCallbackWithFailed(
    TrashServiceImpl::ParseTrashInfoFileCallback callback) {
  InvokeCallbackWithError(base::File::FILE_ERROR_FAILED, std::move(callback));
}

// Extracts and validates the path from a line coming from the `.trashinfo`
// file. Returns an empty path on error.
base::FilePath ValidateAndCreateRestorePath(std::string_view line) {
  // The final newline character should already have been stripped.
  DCHECK(!line.ends_with('\n'));

  if (!line.starts_with(kPathToken)) {
    LOG(ERROR) << "Line does not start with '" << kPathToken << "'";
    return base::FilePath();
  }

  line.remove_prefix(kPathToken.size());

  const std::string unescaped = base::UnescapeBinaryURLComponent(line);
  if (unescaped.size() >= PATH_MAX) {
    LOG(ERROR) << "Extracted path is too long";
    return base::FilePath();
  }

  if (unescaped.find('\0') != std::string::npos) {
    LOG(ERROR) << "Extracted path contains a NUL byte";
    return base::FilePath();
  }

  if (!base::IsStringUTF8(unescaped)) {
    LOG(ERROR) << "Extracted path is not a valid UTF-8 string";
    return base::FilePath();
  }

  const base::FilePath path(std::move(unescaped));

  const std::vector<std::string> components = path.GetComponents();
  base::span<const std::string> parts = components;

  // The first part should be "/".
  if (parts.empty() || parts.front() != "/") {
    LOG(ERROR) << "Extracted path is not absolute";
    return base::FilePath();
  }

  // Pop the first part.
  parts = parts.subspan(1);
  if (parts.empty()) {
    LOG(ERROR) << "Extracted path is just the root path";
    return base::FilePath();
  }

  // Validate each remaining part.
  for (const std::string& part : parts) {
    if (part == "." || part == ".." || part.size() > NAME_MAX) {
      LOG(ERROR) << "Extracted path contains an invalid component";
      return base::FilePath();
    }
  }

  return path;
}

// Extracts and validates the deletion date from a line coming from the
// `.trashinfo` file. Returns a default-created `Time` on error.
base::Time ValidateAndCreateDeletionDate(std::string_view line) {
  // The final newline character should already have been stripped.
  DCHECK(!line.ends_with('\n'));

  if (!line.starts_with(kDeletionDateToken)) {
    LOG(ERROR) << "Line does not start with '" << kDeletionDateToken << "'";
    return base::Time();
  }

  line.remove_prefix(kDeletionDateToken.size());

  base::Time date;
  if (line.size() != kISO8601Size ||
      !base::Time::FromUTCString(std::string(line).c_str(), &date)) {
    LOG(ERROR) << "Cannot parse date";
    return base::Time();
  }

  return date;
}

}  // namespace

TrashServiceImpl::TrashServiceImpl(
    mojo::PendingReceiver<mojom::TrashService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

TrashServiceImpl::~TrashServiceImpl() = default;

void TrashServiceImpl::ParseTrashInfoFile(base::File trash_info_file,
                                          ParseTrashInfoFileCallback callback) {
  if (!trash_info_file.IsValid()) {
    InvokeCallbackWithError(trash_info_file.error_details(),
                            std::move(callback));
    return;
  }

  // Read the file up to the max buffer. In the event of a read error continue
  // trying to parse as this may represent the case where the buffer was
  // exceeded yet `file_contents` contains valid data after that point so
  // continue parsing.
  std::string file_contents;
  base::ScopedFILE read_only_stream(
      base::FileToFILE(std::move(trash_info_file), "r"));

  constexpr size_t kMaxSize = kTrashInfoHeaderToken.size() + kPathToken.size() +
                              PATH_MAX * 3 /* URL-escaping */ +
                              kDeletionDateToken.size() + kISO8601Size +
                              3 /* newline characters */;
  const bool ok = base::ReadStreamToStringWithMaxSize(read_only_stream.get(),
                                                      kMaxSize, &file_contents);
  if (!ok && file_contents.size() < kMaxSize) {
    LOG(ERROR) << "Cannot read trash info file";
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  // Split the lines up and ignoring any empty lines in between. Only the first
  // 3 non-empty lines are useful to validate again.
  std::vector<std::string_view> lines = base::SplitStringPiece(
      file_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.size() < 3) {
    LOG(ERROR) << "Trash info file only contains " << lines.size() << " lines";
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  // The Trash spec says, "The implementation MUST ignore any other lines in
  // this file, except the first line (must be [Trash Info]) and these two
  // key/value pairs". Therefore we only iterate over the first 3 lines ignoring
  // the remaining.
  if (lines[0] != kTrashInfoHeaderToken) {
    LOG(ERROR) << "Invalid trash info header: " << lines[0];
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  base::FilePath restore_path = ValidateAndCreateRestorePath(lines[1]);
  if (restore_path.empty()) {
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  base::Time deletion_date = ValidateAndCreateDeletionDate(lines[2]);
  if (deletion_date.is_null()) {
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  std::move(callback).Run(base::File::FILE_OK, std::move(restore_path),
                          std::move(deletion_date));
}

}  // namespace ash::trash_service
