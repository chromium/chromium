// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMAGE_WRITER_DISK_UNMOUNTER_MAC_H_
#define CHROME_UTILITY_IMAGE_WRITER_DISK_UNMOUNTER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"

namespace image_writer {

// Manages the unmounting of disks through Disk Arbitration.  Disk Arbitration
// has to be run on a thread with a CFRunLoop.  In the utility process neither
// the main or IO thread have one by default, so we need to manage a new thread
// which will explicitly have a CFRunLoop-based message pump.  Note that this
// class can only handle one unmount operation at a time and calling Unmount
// again before the continuation returns will cause undefined behavior.
class DiskUnmounterMac {
 public:
  DiskUnmounterMac();
  ~DiskUnmounterMac();

  // Claims and unmounts the device described by |device_path| and then calls
  // the |continuation| when complete.  This can be called from any thread.
  // The continuation will be run on the thread this object was created on.
  void Unmount(const std::string& device_path,
               base::OnceClosure success_continuation,
               base::OnceClosure failure_continuation);

 private:
  // Handles disk-claimed callbacks.
  static void DiskClaimed(DADiskRef disk,
                          DADissenterRef dissenter,
                          void* context);
  // Handles when we fail to claim a disk.
  static DADissenterRef DiskClaimRevoked(DADiskRef disk, void* context);
  // Handles the disk-unmounted callback.
  static void DiskUnmounted(DADiskRef disk,
                            DADissenterRef dissenter,
                            void* context);

  // Starts the unmount process.  Should be posted to the |cf_thread_|.
  void UnmountOnWorker(const std::string& device_path);

  // A convenience method that triggers the failure continuation.
  void Error();

  scoped_refptr<base::SingleThreadTaskRunner> original_thread_;
  base::OnceClosure success_continuation_;
  base::OnceClosure failure_continuation_;

  base::apple::ScopedCFTypeRef<DADiskRef> disk_;
  base::apple::ScopedCFTypeRef<DASessionRef> session_;

  // Thread is last to ensure it is stopped before the data members are
  // destroyed.
  base::Thread cf_thread_;
};

}  // namespace image_writer

#endif  // CHROME_UTILITY_IMAGE_WRITER_DISK_UNMOUNTER_MAC_H_
