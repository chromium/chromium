// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VRP_FLAGS_VRP_FLAGS_IMPL_H_
#define COMPONENTS_VRP_FLAGS_VRP_FLAGS_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "components/vrp_flags/vrp_flags.h"
#include "components/vrp_flags/vrp_flags.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace vrp_flags {

class COMPONENT_EXPORT(VRP_FLAGS) VrpFlagsImpl : public mojom::VrpFlags {
 public:
  static VrpFlagsImpl* GetInstance();

  VrpFlagsImpl(const VrpFlagsImpl&) = delete;
  VrpFlagsImpl& operator=(const VrpFlagsImpl&) = delete;

  void Bind(mojo::PendingReceiver<mojom::VrpFlags> receiver);

  // mojom::VrpFlags implementation:
  void GetWriteLocations(GetWriteLocationsCallback callback) override;
  void WriteAttempted(uint64_t location,
                      WriteAttemptedCallback callback) override;
  void GetReadPrefix(GetReadPrefixCallback callback) override;
  void ReadAttempted(const base::UnguessableToken& flag,
                     ReadAttemptedCallback callback) override;

  void SetForTesting(bool for_testing) { for_testing_ = for_testing; }
  void SetReadValueForTesting(const base::UnguessableToken& flag);

 private:
  friend class base::NoDestructor<VrpFlagsImpl>;

  VrpFlagsImpl();
  ~VrpFlagsImpl() override;

  struct ReadFlag {
    base::UnguessableToken prefix;
    base::UnguessableToken flag;
  };

  void EnsureAllocated();

  bool for_testing_ = false;
  uint64_t arbitrary_value_ = 0;
  std::vector<base::HeapArray<uint64_t>> write_allocations_;
  // These point into `write_allocations_` and cannot outlive them.
  std::vector<uint64_t*> write_locations_;
  std::unique_ptr<ReadFlag> read_flag_;

  mojo::ReceiverSet<mojom::VrpFlags> receivers_;
};

}  // namespace vrp_flags

#endif  // COMPONENTS_VRP_FLAGS_VRP_FLAGS_IMPL_H_
