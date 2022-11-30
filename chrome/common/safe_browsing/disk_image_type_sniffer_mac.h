// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_DISK_IMAGE_TYPE_SNIFFER_MAC_H_
#define CHROME_COMMON_SAFE_BROWSING_DISK_IMAGE_TYPE_SNIFFER_MAC_H_

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

namespace safe_browsing {

// This class is used to determine whether a given file is a Mac archive type,
// regardless of file extension. It does so by determining whether the file has
// the 'koly' signature typical of Mac archive files.
class DiskImageTypeSnifferMac
    : public base::RefCountedThreadSafe<DiskImageTypeSnifferMac> {
 public:
  DiskImageTypeSnifferMac();

  DiskImageTypeSnifferMac(const DiskImageTypeSnifferMac&) = delete;
  DiskImageTypeSnifferMac& operator=(const DiskImageTypeSnifferMac&) = delete;

  // Reads trailer from file to see if it is a DMG type. Must be called on the
  // FILE thread.
  static bool IsAppleDiskImage(const base::FilePath& dmg_file);

  // Returns true when the trailer is a valid trailer for a DMG type.
  static bool IsAppleDiskImageTrailer(const base::span<const uint8_t>& trailer);

  // The size of a DMG trailer.
  static constexpr size_t kAppleDiskImageTrailerSize = 512;

 private:
  friend class base::RefCountedThreadSafe<DiskImageTypeSnifferMac>;

  ~DiskImageTypeSnifferMac();
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_DISK_IMAGE_TYPE_SNIFFER_MAC_H_
