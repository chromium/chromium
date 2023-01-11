// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/trash_service_impl.h"

#include <linux/limits.h>

#include <utility>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace ash::trash_service {

namespace {

// Represents the expected token on the first line of the .trashinfo file.
constexpr char kTrashInfoHeaderToken[] = "[Trash Info]";

// Represents the expected token starting the second line of the .trashinfo
// file.
constexpr char kPathToken[] = "Path=";

// Represents the expected token starting the third line of the .trashinfo file.
constexpr char kDeletionDateToken[] = "DeletionDate=";

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

// Validates the supplied path and on success updates the `restore_path`
// parameter. On failure function returns false.
base::FilePath ValidateAndCreateRestorePath(base::StringPiece piece) {
  if (!base::StartsWith(piece, kPathToken)) {
    return base::FilePath();
  }

  auto path_without_token =
      base::CollapseWhitespaceASCII(piece.substr(sizeof(kPathToken) - 1),
                                    /*trim_sequences_with_line_breaks=*/true);
  if (path_without_token.size() > PATH_MAX) {
    return base::FilePath();
  }

  base::FilePath restore_path(path_without_token);
  if (restore_path.empty() || !restore_path.IsAbsolute() ||
      restore_path.ReferencesParent()) {
    return base::FilePath();
  }

  return restore_path;
}

// Validates the supplied deletion date and on success updates the
// `deletion_date` parameter. On failure function returns false.
base::Time ValidateAndCreateDeletionDate(base::StringPiece piece) {
  if (!base::StartsWith(piece, kDeletionDateToken)) {
    return base::Time();
  }

  auto date_without_token = base::CollapseWhitespaceASCII(
      piece.substr(sizeof(kDeletionDateToken) - 1),
      /*trim_sequences_with_line_breaks=*/true);
  if (date_without_token.size() != kISO8601Size) {
    return base::Time();
  }

  base::Time deletion_date;
  if (!base::Time::FromUTCString(date_without_token.data(), &deletion_date)) {
    return base::Time();
  }

  return deletion_date;
}

}  // namespace

constexpr size_t kMaxReadBufferInBytes =
    sizeof(kTrashInfoHeaderToken) + sizeof(kPathToken) + PATH_MAX +
    sizeof(kDeletionDateToken) + kISO8601Size + /*new line characters*/ 2;

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
  bool successful_read = base::ReadStreamToStringWithMaxSize(
      read_only_stream.get(), kMaxReadBufferInBytes, &file_contents);
  if (!successful_read && file_contents.size() < kMaxReadBufferInBytes) {
    LOG(ERROR) << "Error reading contents of trash info file";
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  // Split the lines up and ignoring any empty lines in between. Only the first
  // 3 non-empty lines are useful to validate again.
  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      file_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.size() < 3) {
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  // The Trash spec says, "The implementation MUST ignore any other lines in
  // this file, except the first line (must be [Trash Info]) and these two
  // key/value pairs". Therefore we only iterate over the first 3 lines ignoring
  // the remaining.
  if (lines[0] != kTrashInfoHeaderToken) {
    LOG(ERROR) << "Trash info header invalid";
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  base::FilePath restore_path = ValidateAndCreateRestorePath(lines[1]);
  if (restore_path.empty()) {
    LOG(ERROR) << "Path key is invalid";
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  base::Time deletion_date = ValidateAndCreateDeletionDate(lines[2]);
  if (deletion_date.is_null()) {
    LOG(ERROR) << "Deletion date key is invalid";
    InvokeCallbackWithFailed(std::move(callback));
    return;
  }

  std::move(callback).Run(base::File::FILE_OK, std::move(restore_path),
                          std::move(deletion_date));
}

}  // namespace ash::trash_service
