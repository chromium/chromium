// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// MtabWatcherLinux implementation.

#include "components/storage_monitor/mtab_watcher_linux.h"

#include <mntent.h>
#include <stddef.h>
#include <stdio.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"

namespace {

// List of file systems we care about.
const char* const kKnownFileSystems[] = {
  "btrfs",
  "ext2",
  "ext3",
  "ext4",
  "fat",
  "hfsplus",
  "iso9660",
  "msdos",
  "ntfs",
  "udf",
  "vfat",
};

}  // namespace

namespace storage_monitor {

MtabWatcherLinux::MtabWatcherLinux(const base::FilePath& mtab_path,
                                   const UpdateMtabCallback& callback)
    : mtab_path_(mtab_path), callback_(callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool ret = file_watcher_.Watch(
      mtab_path_, base::FilePathWatcher::Type::kNonRecursive,
      base::BindRepeating(&MtabWatcherLinux::OnFilePathChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  if (!ret) {
    LOG(ERROR) << "Adding watch for " << mtab_path_.value() << " failed";
    return;
  }

  ReadMtab();
}

MtabWatcherLinux::~MtabWatcherLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MtabWatcherLinux::ReadMtab() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  FILE* fp = setmntent(mtab_path_.value().c_str(), "r");
  if (!fp)
    return;

  MountPointDeviceMap device_map;
  mntent entry;
  char buf[512];

  // We return the same device mounted to multiple locations, but hide
  // devices that have been mounted over.
  while (getmntent_r(fp, &entry, buf, sizeof(buf))) {
    // We only care about real file systems.
    for (size_t i = 0; i < std::size(kKnownFileSystems); ++i) {
      if (strcmp(kKnownFileSystems[i], entry.mnt_type) == 0) {
        device_map[base::FilePath(entry.mnt_dir)] =
            base::FilePath(entry.mnt_fsname);
        break;
      }
    }
  }
  endmntent(fp);

  callback_.Run(device_map);
}

void MtabWatcherLinux::OnFilePathChanged(
    const base::FilePath& path, bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (path != mtab_path_) {
    // This cannot happen unless FilePathWatcher is buggy. Just ignore this
    // notification and do nothing.
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (error) {
    LOG(ERROR) << "Error watching " << mtab_path_.value();
    return;
  }

  ReadMtab();
}

}  // namespace storage_monitor
