// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/posix_util.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/updater/util/linux_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_util.h"
#endif

namespace updater {
namespace {

bool AdvanceEnumeratorWithStat(base::FileEnumerator* traversal,
                               base::FilePath* out_next_path,
                               base::stat_wrapper_t* out_next_stat) {
  *out_next_path = traversal->Next();
  if (out_next_path->empty()) {
    return false;
  }

  *out_next_stat = traversal->GetInfo().stat();
  return true;
}

}  // namespace

// Recursively delete a folder and its contents, returning `true` on success.
bool DeleteFolder(const std::optional<base::FilePath>& installed_path) {
  if (!installed_path) {
    return false;
  }
  if (!base::DeletePathRecursively(*installed_path)) {
    PLOG(ERROR) << "Deleting " << *installed_path << " failed";
    return false;
  }
  return true;
}

bool DeleteCandidateInstallFolder(UpdaterScope scope) {
  return DeleteFolder(GetVersionedInstallDirectory(scope));
}

bool CopyDir(const base::FilePath& from_path,
             const base::FilePath& to_path,
             bool world_readable) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FileEnumerator traversal(from_path, true,
                                 base::FileEnumerator::FILES |
                                     base::FileEnumerator::SHOW_SYM_LINKS |
                                     base::FileEnumerator::DIRECTORIES);
  base::FilePath current = from_path;
  base::FilePath from_path_base = from_path.DirName();

  base::stat_wrapper_t from_stat = {};
  if (base::File::Stat(from_path, &from_stat) < 0) {
    DPLOG(ERROR) << "Can't stat source directory: " << from_path;
    return false;
  }

  const mode_t mode_executable =
      world_readable ? S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
                     : S_IRWXU;
  const mode_t mode_normal = world_readable
                                 ? S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
                                 : S_IRUSR | S_IWUSR;

  do {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    base::FilePath target_path(to_path);
    if (from_path_base != current &&
        !from_path_base.AppendRelativePath(current, &target_path)) {
      return false;
    }

    if (S_ISDIR(from_stat.st_mode)) {
      if (mkdir(target_path.value().c_str(), mode_executable)) {
        VPLOG(0) << "Can't create directory: " << target_path;
        return false;
      }

      if (chmod(target_path.value().c_str(), mode_executable)) {
        VLOG(0) << "Can't chmod directory: " << target_path;
        return false;
      }
      continue;
    }

    if (S_ISREG(from_stat.st_mode)) {
      base::File infile(open(current.value().c_str(), O_RDONLY));
      if (!infile.IsValid()) {
        VPLOG(0) << "Can't open file: " << current;
        return false;
      }

      const int mode = (from_stat.st_mode & S_IXUSR) == S_IXUSR
                           ? mode_executable
                           : mode_normal;
      base::File outfile(open(target_path.value().c_str(),
                              O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, mode));
      if (!outfile.IsValid()) {
        VPLOG(0) << "Can't create file: " << target_path;
        return false;
      }

      if (fchmod(outfile.GetPlatformFile(), mode)) {
        VLOG(0) << "Can't fchmod file: " << current;
        return false;
      }

      if (!base::CopyFileContents(infile, outfile)) {
        VLOG(0) << "Can't copy file: " << current;
        return false;
      }
      continue;
    }

    if (S_ISLNK(from_stat.st_mode)) {
      char buffer[1000] = {};
      const ssize_t size =
          readlink(current.value().c_str(), buffer, std::size(buffer));
      if (size < 0 || size >= std::ssize(buffer)) {
        VLOG(0) << "Can't read symlink: " << current;
        return false;
      }
      if (symlink(buffer, target_path.value().c_str())) {
        VLOG(0) << "Can't create symlink: " << current;
        return false;
      }
      continue;
    }

    VLOG(0) << current << " is not a directory, regular file, or symlink.";
    return false;
  } while (AdvanceEnumeratorWithStat(&traversal, &current, &from_stat));

  return true;
}

bool WrongUser(UpdaterScope scope) {
  return (scope == UpdaterScope::kSystem) != (geteuid() == 0);
}

bool EulaAccepted(const std::vector<std::string>& app_ids) {
  // On POSIX, there does not exist a way for apps to mark EULA acceptance.
  return false;
}

}  // namespace updater
