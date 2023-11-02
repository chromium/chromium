// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"

namespace metrics {

// static
bool DriveMetricsProvider::HasSeekPenalty(const base::FilePath& path,
                                          bool* has_seek_penalty) {
  struct stat path_stat;
  if (stat(path.value().c_str(), &path_stat) < 0)
    return false;

  const char* dev_name = devname(path_stat.st_dev, S_IFBLK);
  if (!dev_name)
    return false;

  std::string bsd_name("/dev/");
  bsd_name.append(dev_name);

  base::apple::ScopedCFTypeRef<DASessionRef> session(
      DASessionCreate(kCFAllocatorDefault));
  if (!session)
    return false;

  base::apple::ScopedCFTypeRef<DADiskRef> disk(DADiskCreateFromBSDName(
      kCFAllocatorDefault, session.get(), bsd_name.c_str()));
  if (!disk)
    return false;

  base::mac::ScopedIOObject<io_object_t> io_media(
      DADiskCopyIOMedia(disk.get()));
  base::apple::ScopedCFTypeRef<CFDictionaryRef> characteristics(
      static_cast<CFDictionaryRef>(IORegistryEntrySearchCFProperty(
          io_media.get(), kIOServicePlane,
          CFSTR(kIOPropertyDeviceCharacteristicsKey), kCFAllocatorDefault,
          kIORegistryIterateRecursively | kIORegistryIterateParents)));
  if (!characteristics)
    return false;

  CFStringRef type_ref = base::apple::GetValueFromDictionary<CFStringRef>(
      characteristics.get(), CFSTR(kIOPropertyMediumTypeKey));
  if (!type_ref)
    return false;

  NSString* type = base::apple::CFToNSPtrCast(type_ref);
  if ([type isEqualToString:@kIOPropertyMediumTypeRotationalKey]) {
    *has_seek_penalty = true;
    return true;
  }
  if ([type isEqualToString:@kIOPropertyMediumTypeSolidStateKey]) {
    *has_seek_penalty = false;
    return true;
  }

  // TODO(dbeam): should I look for these Rotational/Solid State keys in
  // |characteristics|? What if I find device characteristic but there's no
  // type? Assume rotational?
  return false;
}

}  // namespace metrics
