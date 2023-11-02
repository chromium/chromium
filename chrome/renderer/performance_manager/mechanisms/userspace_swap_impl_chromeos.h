// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_IMPL_CHROMEOS_H_
#define CHROME_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_IMPL_CHROMEOS_H_

#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace performance_manager {
namespace mechanism {

class UserspaceSwapImpl : public userspace_swap::mojom::UserspaceSwap {
 public:
  using MemoryRegionPtr = ::userspace_swap::mojom::MemoryRegionPtr;

  ~UserspaceSwapImpl() override;
  UserspaceSwapImpl();
  UserspaceSwapImpl(const UserspaceSwapImpl&) = delete;
  UserspaceSwapImpl& operator=(const UserspaceSwapImpl&) = delete;

  static void Create(
      mojo::PendingReceiver<userspace_swap::mojom::UserspaceSwap> receiver);

  static bool PlatformSupportsUserspaceSwap();

 protected:
  // UserspaceSwap impl:
  void MovePTEsLeavingMapping(MemoryRegionPtr src, uint64_t dest) override;
  void MapArea(MemoryRegionPtr area) override;
  void GetPartitionAllocSuperPagesUsed(
      int32_t max_superpages,
      GetPartitionAllocSuperPagesUsedCallback callback) override;
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_RENDERER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_IMPL_CHROMEOS_H_
