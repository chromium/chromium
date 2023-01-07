// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_SWAP_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_SWAP_STORAGE_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chromeos/ash/components/memory/userspace_swap/region.h"

namespace ash {
namespace memory {
namespace userspace_swap {

// SwapFile is the implementation for a disk backed swap file. This class is
// thread safe as synchronization is handled internally where necessary.
class COMPONENT_EXPORT(USERSPACE_SWAP) SwapFile {
 public:
  SwapFile(const SwapFile&) = delete;
  SwapFile& operator=(const SwapFile&) = delete;

  virtual ~SwapFile();

  enum Type {
    // kStandard is a normal file without compression or encryption.
    kStandard = 0,
    // kCompressed is an optional compression layer.
    kCompressed = (1 << 1),
    // kEncrypted is an optional encryption layer.
    kEncrypted = (1 << 2),
    // You can use both modes with a bitwise or kCompressed | kEncrypted.
  };

  // Create a new swap file, this can only be called from the browser as
  // renderer seccomp policies would not allow it. Note: kEncrypted is
  // required for ALL swap files, this call will fail without kEncrypted.
  static std::unique_ptr<SwapFile> Create(Type type);

  // GetBackingStoreFreeSpaceKB() returns the number of KB free on the backing
  // device.
  static uint64_t GetBackingStoreFreeSpaceKB();

  // WriteToSwap will write a memory region from |source| into the swap file.
  // Upon successful completion the method will return true and |swap_region|
  // will contain the Region for where it was written in swap. This method will
  // return false on error and errno will be set.
  virtual bool WriteToSwap(const Region& source, Region* swap_region);

  // ReadFromSwap reads the |swap_region| from the swap file writing it into
  // |dest|. The return value is the number of bytes read from the swap file. On
  // error ReadFromSwap will return -1 and errno will be set.
  virtual ssize_t ReadFromSwap(const Region& swap_region, const Region& dest);

  // DropFromSwap can be used to reclaim the disk blocks for |swap_region|, it
  // punches a hole in the file to accomplish this and may not immediately be
  // reflected when the block is still partially in use by another region. This
  // method will return false on failure and errno will be set.
  virtual bool DropFromSwap(const Region& swap_region);

  // GetUsageKB returns the number of KiB the swap file is currently using on
  // disk.
  virtual uint64_t GetUsageKB() const;

  // ReleaseFD is used for donating the internal fd.
  base::ScopedFD ReleaseFD();

 protected:
  static bool GetDirectoryForSwapFile(base::FilePath* file_path);

  // Given an FD to an already open swap file wrap it into a SwapFile class,
  // this is primarily for ease of testing each implementation.
  static std::unique_ptr<SwapFile> WrapFD(base::ScopedFD swap_fd, Type type);

  explicit SwapFile(base::ScopedFD fd);

  base::ScopedFD fd_;

 private:
  friend class SwapStorageTest;

  // We use this lock to serialize WriteToSwap calls. (Concurrent reads and
  // drops are safe, because they use syscalls which do not rely on the file
  // pointer, specifically pread(2) and fallocate(2) respectively).
  base::Lock write_lock_;
};

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_SWAP_STORAGE_H_
