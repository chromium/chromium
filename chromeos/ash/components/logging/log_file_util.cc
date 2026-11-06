// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/logging/log_file_util.h"

#include <unistd.h>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace ash {

namespace {

// The directory on which we do rotation of log files instead of switching
// with symlink. Because this directory doesn't support symlinks and the logic
// doesn't work correctly.
constexpr char kChronosHomeDir[] = "/home/chronos/user/";

}  // namespace

bool RotateLogFile(const base::FilePath& target_path) {
  DCHECK(!target_path.empty());
  // If the old log file doesn't exist, do nothing.
  if (!base::PathExists(target_path)) {
    return true;
  }

  // Retrieve the creation time of the old log file.
  base::File::Info info;
  {
    // Opens a file, only if it exists.
    base::File fp(target_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!fp.IsValid() || !fp.GetInfo(&info)) {
      // On failure, keep using the same file.
      return false;
    }
  }

  // Generate the rotated log path name from the creation time.
  // (eg. "/home/chrome/user/log/chrome_220102-030405")
  base::Time timestamp = info.creation_time;
  base::FilePath rotated_path = GenerateTimestampedName(target_path, timestamp);

  // Rare case: if the target path already exists, generate the alternative by
  // incrementing the timestamp. This may happen when the Chrome restarts
  // multiple times in a second.
  while (base::PathExists(rotated_path)) {
    timestamp += base::Seconds(1);
    rotated_path = GenerateTimestampedName(target_path, timestamp);
  }

  // Rename the old log file: |target_path| => |rotated_path|.
  // We don't use |base::Move|, since we don't consider the inter-filesystem
  // move in this logic. The current logic depends on the fact that the ctime
  // won't be changed after rotation, but ctime may be changed on
  // inter-filesystem move.
  if (!base::ReplaceFile(target_path, rotated_path, nullptr)) {
    PLOG(ERROR) << "Failed to rotate the log files: " << target_path << " => "
                << rotated_path;
    return false;
  }

  return true;
}

base::FilePath SetUpSymlinkIfNeeded(const base::FilePath& symlink_path,
                                    bool new_log) {
  DCHECK(!symlink_path.empty());
  // For backward compatibility, set up a .../chrome symlink to
  // .../chrome.LATEST as needed.  This code needs to run only
  // after the migration (i.e. the addition of chrome.LATEST).
  if (symlink_path.Extension() == ".LATEST") {
    base::FilePath extensionless_path = symlink_path.ReplaceExtension("");
    base::FilePath target_path;
    bool extensionless_symlink_exists =
        base::ReadSymbolicLink(extensionless_path, &target_path);

    if (target_path != symlink_path) {
      // No link, or wrong link.  Clean up.  This should happen only once in
      // each log directory after the OS version update, but some of those
      // directories may not be accessed for a long time, so this code needs to
      // stay in forever :/
      if (extensionless_symlink_exists &&
          !base::DeleteFile(extensionless_path)) {
        DPLOG(WARNING) << "Cannot delete " << extensionless_path.value();
      }
      // After cleaning up, create the symlink.
      if (!base::CreateSymbolicLink(symlink_path, extensionless_path)) {
        DPLOG(ERROR) << "Cannot create " << extensionless_path.value();
      }
    }
  }

  // If not starting a new log, then just log through the existing symlink, but
  // if the symlink doesn't exist, create it.
  //
  // If starting a new log, then rename the old symlink as
  // symlink_path.PREVIOUS and make a new symlink to a fresh log file.

  // Check for existence of the symlink.
  base::FilePath target_path;
  bool symlink_exists = base::ReadSymbolicLink(symlink_path, &target_path);

  if (symlink_exists && !new_log) {
    return target_path;
  }

  // Remove any extension before time-stamping.
  target_path = GenerateTimestampedName(symlink_path.RemoveExtension(),
                                        base::Time::Now());

  if (symlink_exists) {
    base::FilePath previous_symlink_path =
        symlink_path.ReplaceExtension(".PREVIOUS");
    // Rename symlink to .PREVIOUS.  This nukes an existing symlink just like
    // the rename(2) syscall does.
    if (!base::ReplaceFile(symlink_path, previous_symlink_path, nullptr)) {
      DPLOG(WARNING) << "Cannot rename " << symlink_path.value() << " to "
                     << previous_symlink_path.value();
    }
  }
  // If all went well, the symlink no longer exists.  Recreate it.
  base::FilePath relative_target_path = target_path.BaseName();
  if (!base::CreateSymbolicLink(relative_target_path, symlink_path)) {
    DPLOG(ERROR) << "Unable to create symlink " << symlink_path.value()
                 << " pointing at " << relative_target_path.value();
  }
  return target_path;
}

void RemoveSymlinkAndLog(const base::FilePath& link_path,
                         const base::FilePath& target_path) {
  if (::unlink(link_path.value().c_str()) == -1) {
    DPLOG(WARNING) << "Unable to unlink symlink " << link_path.value();
  }
  if (target_path != link_path && ::unlink(target_path.value().c_str()) == -1) {
    DPLOG(WARNING) << "Unable to unlink log file " << target_path.value();
  }
}

base::FilePath SetUpLogFile(const base::FilePath& target_path, bool new_log) {
  const bool supports_symlinks =
      !(target_path.IsAbsolute() &&
        base::StartsWith(target_path.value(), kChronosHomeDir));

  // TODO(crbug.com/40225776): Remove the old symlink logic.
  if (supports_symlinks) {
    // As for now, we keep the original log rotation logic on the file system
    // which supports symlinks.
    return SetUpSymlinkIfNeeded(target_path, new_log);
  }

  // Chrome OS doesn't support symlinks on this file system, so that it uses
  // the rotation logic which doesn't use symlinks.
  if (!new_log) {
    // Keep using the same log file without doing anything.
    return target_path;
  }

  // For backward compatibility, ignore a ".LATEST" extension the way
  // |SetUpSymlinkIfNeeded()| does.
  base::FilePath bare_path = target_path;
  if (target_path.Extension() == ".LATEST") {
    bare_path = target_path.ReplaceExtension("");
  }

  // Try to rotate the log.
  if (!RotateLogFile(bare_path)) {
    PLOG(ERROR) << "Failed to rotate the log file: " << bare_path.value()
                << ". Keeping using the same log file without rotating.";
  }

  return bare_path;
}

base::FilePath GenerateTimestampedName(const base::FilePath& base_path,
                                       base::Time timestamp) {
  base::Time::Exploded exploded;
  timestamp.UTCExplode(&exploded);
  return base_path.InsertBeforeExtensionASCII(base::StringPrintf(
      "_%02d%02d%02d-%02d%02d%02d", exploded.year % 100, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second));
}

}  // namespace ash
