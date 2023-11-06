// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/image_writer/image_writer_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/sys_string_conversions.h"

namespace extensions {

namespace {

bool IsUsbDevice(io_object_t disk_obj) {
  io_object_t current_obj = disk_obj;
  io_object_t parent_obj = 0;
  // Keep scoped object outside the loop so the object lives to the next
  // GetParentEntry.
  base::mac::ScopedIOObject<io_object_t> parent_obj_ref(parent_obj);

  while ((IORegistryEntryGetParentEntry(
             current_obj, kIOServicePlane, &parent_obj)) == KERN_SUCCESS) {
    current_obj = parent_obj;
    parent_obj_ref.reset(parent_obj);

    base::apple::ScopedCFTypeRef<CFStringRef> class_name(
        IOObjectCopyClass(current_obj));
    if (!class_name) {
      LOG(ERROR) << "Could not get object class of IO Registry Entry.";
      continue;
    }

    if (CFStringCompare(class_name.get(), CFSTR("IOUSBMassStorageClass"), 0) ==
        kCFCompareEqualTo) {
      return true;
    }
  }

  return false;
}

}  // namespace

bool IsSuitableRemovableStorageDevice(io_object_t disk_obj,
                                      std::string* out_bsd_name,
                                      uint64_t* out_size_in_bytes,
                                      bool* out_removable) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result = IORegistryEntryCreateCFProperties(
      disk_obj, dict.InitializeInto(), kCFAllocatorDefault, 0);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "Unable to get properties of disk object.";
    return false;
  }

  // Do not allow Core Storage volumes, even though they are marked as "whole
  // media", as they are entirely contained on a different volume.
  CFBooleanRef cf_corestorage =
      base::apple::GetValueFromDictionary<CFBooleanRef>(dict.get(),
                                                        CFSTR("CoreStorage"));
  if (cf_corestorage && CFBooleanGetValue(cf_corestorage))
    return false;

  // Do not allow APFS containers, even though they are marked as "whole
  // media", as they are entirely contained on a different volume.
  CFStringRef cf_content = base::apple::GetValueFromDictionary<CFStringRef>(
      dict.get(), CFSTR("Content"));
  if (cf_content &&
      CFStringCompare(cf_content, CFSTR("EF57347C-0000-11AA-AA11-00306543ECAC"),
                      0) == kCFCompareEqualTo) {
    return false;
  }

  CFBooleanRef cf_removable = base::apple::GetValueFromDictionary<CFBooleanRef>(
      dict.get(), CFSTR(kIOMediaRemovableKey));
  bool removable = CFBooleanGetValue(cf_removable);
  bool is_usb = IsUsbDevice(disk_obj);

  if (!removable && !is_usb)
    return false;

  if (out_size_in_bytes) {
    CFNumberRef cf_media_size =
        base::apple::GetValueFromDictionary<CFNumberRef>(
            dict.get(), CFSTR(kIOMediaSizeKey));
    if (cf_media_size)
      CFNumberGetValue(cf_media_size, kCFNumberLongLongType, out_size_in_bytes);
    else
      *out_size_in_bytes = 0;
  }

  if (out_bsd_name) {
    CFStringRef cf_bsd_name = base::apple::GetValueFromDictionary<CFStringRef>(
        dict.get(), CFSTR(kIOBSDNameKey));
    if (out_bsd_name)
      *out_bsd_name = base::SysCFStringRefToUTF8(cf_bsd_name);
    else
      *out_bsd_name = std::string();
  }

  if (out_removable)
    *out_removable = removable;

  return true;
}

}  // namespace extensions
