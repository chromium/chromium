// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_old_versions.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/version.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

using PathVector = std::vector<base::FilePath>;
using DirectorySet = std::set<base::FilePath>;
using ExecutableMap = std::map<base::FilePath, PathVector>;

// Returns the name of the version directory for executable |exe_path|.
base::FilePath GetExecutableVersionDirName(const base::FilePath& exe_path) {
  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfo(exe_path));
  if (!file_version_info.get())
    return base::FilePath();
  return base::FilePath::FromUTF16Unsafe(file_version_info->file_version());
}

// Returns the names of the old version directories found in |install_dir|. The
// directories named after the version of chrome.exe or new_chrome.exe are
// excluded.
DirectorySet GetOldVersionDirectories(const base::FilePath& install_dir) {
  // TODO(crbug.com/40171016): Delete old version directory from all known
  // locations.
  const base::FilePath new_chrome_exe_version_dir_name =
      GetExecutableVersionDirName(install_dir.Append(kChromeNewExe));
  const base::FilePath chrome_exe_version_dir_name =
      GetExecutableVersionDirName(install_dir.Append(kChromeExe));

  DirectorySet directories;
  base::FileEnumerator enum_directories(install_dir, false,
                                        base::FileEnumerator::DIRECTORIES);
  for (base::FilePath directory_path = enum_directories.Next();
       !directory_path.empty(); directory_path = enum_directories.Next()) {
    const base::FilePath directory_name = directory_path.BaseName();
    const base::Version version(directory_name.AsUTF8Unsafe());
    const size_t kNumChromeVersionComponents = 4;
    if (version.IsValid() &&
        version.components().size() == kNumChromeVersionComponents &&
        directory_name != new_chrome_exe_version_dir_name &&
        directory_name != chrome_exe_version_dir_name) {
      directories.insert(directory_name);
    }
  }
  return directories;
}

// Returns a map where the keys are version directory names and values are paths
// of old_chrome*.exe executables found in |install_dir|.
ExecutableMap GetOldExecutables(const base::FilePath& install_dir) {
  ExecutableMap executables;
  base::FileEnumerator enum_executables(install_dir, false,
                                        base::FileEnumerator::FILES,
                                        FILE_PATH_LITERAL("old_chrome*.exe"));
  for (base::FilePath exe_path = enum_executables.Next(); !exe_path.empty();
       exe_path = enum_executables.Next()) {
    executables[GetExecutableVersionDirName(exe_path)].push_back(exe_path);
  }
  return executables;
}

// Deletes directories that are in |directories| and don't have a matching
// executable in |executables|. Returns false if any such directories could not
// be deleted.
bool DeleteDirectoriesWithoutMatchingExecutable(
    const DirectorySet& directories,
    const ExecutableMap& executables,
    const base::FilePath& install_dir) {
  bool success = true;
  for (const base::FilePath& directory_name : directories) {
    // Delete the directory if it doesn't have a matching executable.
    if (!base::Contains(executables, directory_name)) {
      const base::FilePath directory_path = install_dir.Append(directory_name);
      LOG(WARNING) << "Attempting to delete stray directory "
                   << directory_path.value();
      if (!base::DeletePathRecursively(directory_path)) {
        PLOG(ERROR) << "Failed to delete stray directory "
                    << directory_path.value();
        success = false;
      }
    }
  }
  return success;
}

// Deletes executables that are in |executables| and don't have a matching
// directory in |directories|. Returns false if any such files could not be
// deleted.
bool DeleteExecutablesWithoutMatchingDirectory(
    const DirectorySet& directories,
    const ExecutableMap& executables) {
  bool success = true;
  for (const auto& version_and_executables : executables) {
    const auto& version_dir_name = version_and_executables.first;
    const auto& executables_for_version = version_and_executables.second;

    // Don't delete the executables if they have a matching directory.
    if (base::Contains(directories, version_dir_name))
      continue;

    // Delete executables for version |version_dir_name|.
    for (const auto& executable_path : executables_for_version) {
      const base::FilePath executable_name = executable_path.BaseName();
      LOG(WARNING) << "Attempting to delete stray executable "
                   << executable_path.value();
      if (!base::DeleteFile(executable_path)) {
        PLOG(ERROR) << "Failed to delete stray executable "
                    << executable_path.value();
        success = false;
      }
    }
  }
  return success;
}

// Opens |path| with options that prevent the file from being read or written
// via another handle. As long as the returned object is alive, it is guaranteed
// that |path| isn't in use. It can however be deleted.
base::File GetFileLock(const base::FilePath& path) {
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_WIN_EXCLUSIVE_READ |
                              base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                              base::File::FLAG_WIN_SHARE_DELETE);
}

// Deletes |version_directory| and all executables in |version_executables| if
// no .exe or .dll file for the version is in use. Returns false if any file
// or directory for the version could not be deleted.
bool DeleteVersion(const base::FilePath& version_directory,
                   const PathVector& version_executables) {
  std::vector<base::File> locks;
  PathVector locked_file_paths;

  // Lock .exe/.dll files in |version_directory|.
  base::FileEnumerator enum_version_directory(version_directory, true,
                                              base::FileEnumerator::FILES);
  for (base::FilePath path = enum_version_directory.Next(); !path.empty();
       path = enum_version_directory.Next()) {
    if (!path.MatchesExtension(FILE_PATH_LITERAL(".exe")) &&
        !path.MatchesExtension(FILE_PATH_LITERAL(".dll"))) {
      continue;
    }
    locks.push_back(GetFileLock(path));
    if (!locks.back().IsValid()) {
      LOG(WARNING) << "Failed to delete old version "
                   << version_directory.value() << " because " << path.value()
                   << " is in use.";
      return false;
    }
    locked_file_paths.push_back(path);
  }

  // Lock executables in |version_executables|.
  for (const base::FilePath& executable_path : version_executables) {
    locks.push_back(GetFileLock(executable_path));
    if (!locks.back().IsValid()) {
      LOG(WARNING) << "Failed to delete old version "
                   << version_directory.value() << " because "
                   << executable_path.value() << " is in use.";
      return false;
    }
    locked_file_paths.push_back(executable_path);
  }

  bool success = true;

  // Delete locked files. The files won't actually be deleted until the locks
  // are released.
  for (const base::FilePath& locked_file_path : locked_file_paths) {
    if (!base::DeleteFile(locked_file_path)) {
      PLOG(ERROR) << "Failed to delete locked file "
                  << locked_file_path.value();
      success = false;
    }
  }

  // Release the locks, causing the locked files to actually be deleted. The
  // version directory can't be deleted before this is done.
  locks.clear();

  // Delete the version directory.
  if (!base::DeletePathRecursively(version_directory)) {
    PLOG(ERROR) << "Failed to delete version directory "
                << version_directory.value();
    success = false;
  }

  return success;
}

// For each executable in |executables| that has a matching directory in
// |directories|, tries to delete the executable and the matching directory. No
// deletion occurs for a given version if a .exe or .dll file for that version
// is in use. Returns false if any directory/executables pair could not be
// deleted.
bool DeleteMatchingExecutablesAndDirectories(
    const DirectorySet& directories,
    const ExecutableMap& executables,
    const base::FilePath& install_dir) {
  bool success = true;
  for (const auto& directory_name : directories) {
    // Don't delete the version unless the directory has at least one matching
    // executable.
    auto version_executables_it = executables.find(directory_name);
    if (version_executables_it == executables.end())
      continue;

    // Try to delete all files for the version.
    success &= DeleteVersion(install_dir.Append(directory_name),
                             version_executables_it->second);
  }
  return success;
}

}  // namespace

bool DeleteOldVersions(const base::FilePath& install_dir) {
  const DirectorySet old_directories = GetOldVersionDirectories(install_dir);
  const ExecutableMap old_executables = GetOldExecutables(install_dir);

  bool success = true;
  success &= DeleteDirectoriesWithoutMatchingExecutable(
      old_directories, old_executables, install_dir);
  success &= DeleteExecutablesWithoutMatchingDirectory(old_directories,
                                                       old_executables);
  success &= DeleteMatchingExecutablesAndDirectories(
      old_directories, old_executables, install_dir);

  return success;
}

}  // namespace installer
