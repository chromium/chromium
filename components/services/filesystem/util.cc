// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <limits>
#include <string>

#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

// module filesystem has various constants which must line up with enum values
// in base::File::Flags.
static_assert(filesystem::mojom::kFlagOpen ==
                  static_cast<uint32_t>(base::File::FLAG_OPEN),
              "");
static_assert(filesystem::mojom::kFlagCreate ==
                  static_cast<uint32_t>(base::File::FLAG_CREATE),
              "");
static_assert(filesystem::mojom::kFlagOpenAlways ==
                  static_cast<uint32_t>(base::File::FLAG_OPEN_ALWAYS),
              "");
static_assert(filesystem::mojom::kCreateAlways ==
                  static_cast<uint32_t>(base::File::FLAG_CREATE_ALWAYS),
              "");
static_assert(filesystem::mojom::kFlagOpenTruncated ==
                  static_cast<uint32_t>(base::File::FLAG_OPEN_TRUNCATED),
              "");
static_assert(filesystem::mojom::kFlagRead ==
                  static_cast<uint32_t>(base::File::FLAG_READ),
              "");
static_assert(filesystem::mojom::kFlagWrite ==
                  static_cast<uint32_t>(base::File::FLAG_WRITE),
              "");
static_assert(filesystem::mojom::kFlagAppend ==
                  static_cast<uint32_t>(base::File::FLAG_APPEND),
              "");

// filesystem.Whence in types.mojom must be the same as base::File::Whence.
static_assert(static_cast<int>(filesystem::mojom::Whence::FROM_BEGIN) ==
                  static_cast<int>(base::File::FROM_BEGIN),
              "");
static_assert(static_cast<int>(filesystem::mojom::Whence::FROM_CURRENT) ==
                  static_cast<int>(base::File::FROM_CURRENT),
              "");
static_assert(static_cast<int>(filesystem::mojom::Whence::FROM_END) ==
                  static_cast<int>(base::File::FROM_END),
              "");

namespace filesystem {

base::File::Error IsWhenceValid(mojom::Whence whence) {
  return (whence == mojom::Whence::FROM_CURRENT ||
          whence == mojom::Whence::FROM_BEGIN ||
          whence == mojom::Whence::FROM_END)
             ? base::File::Error::FILE_OK
             : base::File::Error::FILE_ERROR_INVALID_OPERATION;
}

base::File::Error IsOffsetValid(int64_t offset) {
  return (offset >= std::numeric_limits<off_t>::min() &&
          offset <= std::numeric_limits<off_t>::max())
             ? base::File::Error::FILE_OK
             : base::File::Error::FILE_ERROR_INVALID_OPERATION;
}

base::File::Error GetError(const base::File& file) {
  return file.error_details();
}

mojom::FileInformationPtr MakeFileInformation(const base::File::Info& info) {
  mojom::FileInformationPtr file_info(mojom::FileInformation::New());
  file_info->type = info.is_directory ? mojom::FsFileType::DIRECTORY
                                      : mojom::FsFileType::REGULAR_FILE;
  file_info->size = info.size;

  file_info->atime = info.last_accessed.InSecondsFSinceUnixEpoch();
  file_info->mtime = info.last_modified.InSecondsFSinceUnixEpoch();
  file_info->ctime = info.creation_time.InSecondsFSinceUnixEpoch();

  return file_info;
}

base::File::Error ValidatePath(const std::string& raw_path,
                               const base::FilePath& filesystem_base,
                               base::FilePath* out) {
  if (!base::IsStringUTF8(raw_path))
    return base::File::Error::FILE_ERROR_INVALID_OPERATION;

#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType path = base::UTF8ToWide(raw_path);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::FilePath::StringType path = raw_path;
#endif

  // TODO(erg): This isn't really what we want. FilePath::AppendRelativePath()
  // is closer. We need to deal with entirely hostile apps trying to bust this
  // function to use a possibly maliciously provided |raw_path| to bust out of
  // |filesystem_base|.
  base::FilePath full_path = filesystem_base.Append(path);
  if (full_path.ReferencesParent()) {
    // TODO(erg): For now, if it references a parent, we'll consider this bad.
    return base::File::Error::FILE_ERROR_ACCESS_DENIED;
  }

  *out = full_path;
  return base::File::Error::FILE_OK;
}

}  // namespace filesystem
