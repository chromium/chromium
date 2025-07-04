// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/backend_params_manager.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

namespace {

#if BUILDFLAG(IS_WIN)
const uint32_t kMaxFilePathLength = MAX_PATH - 1;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
const uint32_t kMaxFilePathLength = PATH_MAX - 1;
#endif

struct FilePathWithInfo {
  base::FilePath file_path;
  base::File::Info info;
};

const base::FilePath::CharType kDbFile[] = FILE_PATH_LITERAL(".db_file");
const base::FilePath::CharType kJournalFile[] =
    FILE_PATH_LITERAL(".journal_file");

constexpr size_t kLruCacheCapacity = 100;

// All characters allowed in filenames.
constexpr std::string_view kAllowedCharsInFilenames =
    "abcdefghijklmnopqrstuvwxyz0123456789-._~"
    "#[]@!$&'()+,;=";

// Use to translate a character `c` viable for a filename into another arbitrary
// but equally viable character. To reverse the process the function is called
// with the opposite value for `forward`. If `c` is invalid empty is returned.
std::optional<char> RotateChar(char c, bool forward) {
  static_assert(kAllowedCharsInFilenames.length() < 128,
                "Allowed chars are a subset of ASCII and overflow while "
                "indexing should never be a worry");
  size_t char_index = kAllowedCharsInFilenames.find(c);

  // Characters illegal in filenames are not handled in this function.
  if (char_index == std::string::npos) {
    return std::nullopt;
  }

  // Arbitrary offset to rotate index in the list of allowed characters.
  constexpr int64_t kRotationOffset = 37;

  // Use a rotating index to find a character to replace `c`. Using XOR is not
  // viable because it doesn't always give a character that is viable in a
  // filename.
  if (forward) {
    return kAllowedCharsInFilenames[(char_index + kRotationOffset) %
                                    kAllowedCharsInFilenames.length()];
  }
  return kAllowedCharsInFilenames[(char_index +
                                   kAllowedCharsInFilenames.length() -
                                   kRotationOffset) %
                                  kAllowedCharsInFilenames.length()];
}

// Mapping of characters illegal in filenames to a unique token to represent
// them in filenames. This prevents collisions by avoiding two characters get
// mapped to the same value. Ex:
// "*/" --> " 9 2"
// "><" --> " 5 4"
//
// Mapping both strings to " 1 1" for example would result in a valid filename
// but in backing files being shared for two keys which is not correct.
static_assert(kAllowedCharsInFilenames.find(' ') == std::string::npos,
              "Space is not allowed in filenames by itself.");
using ConstStringPair = std::pair<char, const char*>;
std::array<ConstStringPair, 10> kCharacterToTokenMap{
    ConstStringPair{'\\', " 1"}, ConstStringPair{'/', " 2"},
    ConstStringPair{'|', " 3"},  ConstStringPair{'<', " 4"},
    ConstStringPair{'>', " 5"},  ConstStringPair{':', " 6"},
    ConstStringPair{'\"', " 7"}, ConstStringPair{'?', " 8"},
    ConstStringPair{'*', " 9"},  ConstStringPair{'\n', " 0"}};

// Use to get a token to insert in a filename if `c` is a character
// illegal in filenames and an empty string if it's not.
std::string_view FilenameIllegalCharToReplacementToken(char c) {
  for (const auto& pair : kCharacterToTokenMap) {
    if (c == pair.first) {
      return pair.second;
    }
  }
  return "";
}

// Use to get a character associated with `token` if it exists and empty
// if it doesn't.
std::optional<char> ReplacementTokenToFilenameIllegalChar(
    std::string_view token) {
  for (const auto& pair : kCharacterToTokenMap) {
    if (token == pair.second) {
      return pair.first;
    }
  }

  return {};
}

}  // namespace

namespace persistent_cache {

BackendParamsManager::BackendParamsManager(base::FilePath top_directory)
    : backend_params_map_(kLruCacheCapacity),
      top_directory_(std::move(top_directory)) {
  if (!base::PathExists(top_directory_)) {
    base::CreateDirectory(top_directory_);
  }
}
BackendParamsManager::~BackendParamsManager() = default;

void BackendParamsManager::GetParamsSyncOrCreateAsync(
    BackendType backend_type,
    const std::string& key,
    AccessRights access_rights,
    CompletedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = backend_params_map_.Get(
      BackendParamsKey{.backend_type = backend_type, .key = key});
  if (it != backend_params_map_.end()) {
    std::move(callback).Run(it->second);
    return;
  }

  std::string filename = FileNameFromKey(key);
  if (filename.empty()) {
    std::move(callback).Run(BackendParams());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BackendParamsManager::CreateParamsSync, top_directory_,
                     backend_type, key, access_rights),
      base::BindOnce(&BackendParamsManager::SaveParams,
                     weak_factory_.GetWeakPtr(), key, std::move(callback)));
}

BackendParams BackendParamsManager::GetOrCreateParamsSync(
    BackendType backend_type,
    const std::string& key,
    AccessRights access_rights) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = backend_params_map_.Get(
      BackendParamsKey{.backend_type = backend_type, .key = key});
  if (it != backend_params_map_.end()) {
    return it->second.Copy();
  }

  std::string filename = FileNameFromKey(key);
  if (filename.empty()) {
    return BackendParams();
  }

  BackendParams new_params =
      CreateParamsSync(top_directory_, backend_type, filename, access_rights);
  SaveParams(key, CompletedCallback(), new_params.Copy());

  return new_params;
}

void BackendParamsManager::DeleteAllFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear params cache so they don't hold on to files or prevent their
  // deletion. BackendParam instances that were vended by this class and
  // retained somewhere else can still create problems and need to be handled
  // appropriately.
  backend_params_map_.Clear();

  base::DeletePathRecursively(top_directory_);

  // Recreate the directory since the objective was to delete files only.
  base::CreateDirectory(top_directory_);
}

FootprintReductionResult BackendParamsManager::BringDownTotalFootprintOfFiles(
    int64_t target_footprint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear params cache so they don't hold on to files or prevent their
  // deletion. BackendParam instances that were vended by this class and
  // retained somewhere else can still create problems and need to be handled
  // appropriately.
  backend_params_map_.Clear();

  int64_t total_footprint = 0;
  std::vector<FilePathWithInfo> filepaths_with_info;
  base::FileEnumerator file_enumerator(top_directory_, /*recursive=*/false,
                                       base::FileEnumerator::FILES);

  file_enumerator.ForEach([&total_footprint, &filepaths_with_info](
                              const base::FilePath& file_path) {
    base::File::Info info;
    base::GetFileInfo(file_path, &info);

    // Only target database files for deletion.
    if (file_path.MatchesFinalExtension(kDbFile)) {
      filepaths_with_info.emplace_back(file_path, info);
    }

    // All files count towards measured footprint.
    total_footprint += info.size;
  });

  // Nothing to do.
  if (total_footprint <= target_footprint) {
    return FootprintReductionResult{.current_footprint = total_footprint,
                                    .number_of_bytes_deleted = 0};
  }

  // Order files from least to most recently modified to prioritize deleting
  // older staler files.
  std::sort(filepaths_with_info.begin(), filepaths_with_info.end(),
            [](const FilePathWithInfo& left, const FilePathWithInfo& right) {
              return left.info.last_modified < right.info.last_modified;
            });

  int64_t size_of_necessary_deletes = total_footprint - target_footprint;
  int64_t deleted_size = 0;

  for (const FilePathWithInfo& file_path_with_info : filepaths_with_info) {
    if (size_of_necessary_deletes <= deleted_size) {
      break;
    }

    bool db_file_delete_success =
        base::DeleteFile(file_path_with_info.file_path);
    base::UmaHistogramBoolean(
        "PersistentCache.ParamsManager.DbFile.DeleteSucess",
        db_file_delete_success);

    if (db_file_delete_success) {
      deleted_size += file_path_with_info.info.size;

      base::FilePath journal_file_path =
          file_path_with_info.file_path.ReplaceExtension(kJournalFile);
      base::File::Info journal_file_info;
      base::GetFileInfo(journal_file_path, &journal_file_info);

      // TODO (https://crbug.com/377475540): Cleanup when deletion of journal
      // failed.
      bool journal_file_delete_success = base::DeleteFile(journal_file_path);
      base::UmaHistogramBoolean(
          "PersistentCache.ParamsManager.JournalFile.DeleteSucess",
          journal_file_delete_success);

      if (journal_file_delete_success) {
        deleted_size += journal_file_info.size;
      }
    };
  }

  return FootprintReductionResult{
      .current_footprint = total_footprint - deleted_size,
      .number_of_bytes_deleted = deleted_size};
}

// static
std::string BackendParamsManager::FileNameFromKey(const std::string& key) {
  std::string filename;
  filename.reserve(key.size());

  for (char c : key) {
    std::string_view token = FilenameIllegalCharToReplacementToken(c);
    if (!token.empty()) {
      filename += token;
    } else {
      std::optional<char> rotated_char = RotateChar(c, true);

      if (!rotated_char.has_value()) {
        // There's no way to rotate an illegal character so return an empty
        // string.
        return "";
      }
      filename += rotated_char.value();
    }
  }

  return filename;
}

// static
std::string BackendParamsManager::KeyFromFileName(const std::string& filename) {
  std::string key;
  key.reserve(filename.size());

  for (auto it = filename.begin(); it != filename.end(); ++it) {
    if (*it == ' ') {
      // Spaces cannot be by themselves in filenames. Return an empty string
      // instead of CHECKing here because it's not advisable to have a crash
      // because something renamed a file.
      if (it + 1 == filename.end()) {
        return "";
      }

      std::optional<char> c =
          ReplacementTokenToFilenameIllegalChar(std::string_view(it, it + 2));
      if (c.has_value()) {
        key += c.value();

        // Skip the character already parsed.
        ++it;
        continue;
      }

      // If executiion gets here it's that a space was followed by a character
      // that didn't resolve to anything. This means the file name is invalid.
      return "";
    } else {
      std::optional<char> rotated_char = RotateChar(*it, false);

      if (!rotated_char.has_value()) {
        // There's no way to rotate an illegal character so return an empty
        // string.
        return "";
      }

      key += rotated_char.value();
    }
  }

  return key;
}

// static
BackendParams BackendParamsManager::CreateParamsSync(
    base::FilePath directory,
    BackendType backend_type,
    const std::string& filename,
    AccessRights access_rights) {
  BackendParams params;
  params.type = backend_type;

  const bool writes_supported = (access_rights == AccessRights::kReadWrite);
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ;

  if (writes_supported) {
    flags |= base::File::FLAG_WRITE;
  }

#if BUILDFLAG(IS_WIN)
  // PersistentCache backing files are not executables.
  flags |= base::File::FLAG_WIN_NO_EXECUTE;

  // String conversion to wstring necessary on Windows.
  std::wstring filename_part = base::UTF8ToWide(filename);
  base::FilePath db_file_name =
      base::FilePath(base::StrCat({filename_part, kDbFile}));
  base::FilePath journal_file_name =
      base::FilePath(base::StrCat({filename_part, kJournalFile}));
#else
  base::FilePath db_file_name =
      base::FilePath(base::StrCat({filename, kDbFile}));
  base::FilePath journal_file_name =
      base::FilePath(base::StrCat({filename, kJournalFile}));
#endif

  base::FilePath db_file_full_path = directory.Append(db_file_name);
  params.db_file = base::File(db_file_full_path, flags);
  params.db_file_is_writable = writes_supported;

  base::FilePath journal_file_full_path = directory.Append(journal_file_name);
  params.journal_file = base::File(journal_file_full_path, flags);
  params.journal_file_is_writable = writes_supported;

  if (!params.db_file.IsValid() || !params.journal_file.IsValid()) {
    size_t smallest_path_length =
        std::min(db_file_full_path.value().length(),
                 journal_file_full_path.value().length());
    if (smallest_path_length > kMaxFilePathLength) {
      base::UmaHistogramCounts100(
          "PersistentCache.ParamsManager.FilenameCharactersOverLimit",
          smallest_path_length - kMaxFilePathLength);
    }
  }

  return params;
}

void BackendParamsManager::SaveParams(const std::string& key,
                                      CompletedCallback callback,
                                      BackendParams backend_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (callback) {
    std::move(callback).Run(backend_params);
  }

  // Avoid saving invalid files.
  if (backend_params.db_file.IsValid() &&
      backend_params.journal_file.IsValid()) {
    backend_params_map_.Put(
        BackendParamsKey{.backend_type = backend_params.type, .key = key},
        std::move(backend_params));
  }
}

// static
std::string BackendParamsManager::GetAllAllowedCharactersInKeysForTesting() {
  // Start with all characters allowed in both keys and filenames.
  std::string allowed_characters(kAllowedCharsInFilenames);

  // Add characters only allowed in keys.
  for (const auto& pair : kCharacterToTokenMap) {
    allowed_characters += pair.first;
  }

  return allowed_characters;
}

}  // namespace persistent_cache
