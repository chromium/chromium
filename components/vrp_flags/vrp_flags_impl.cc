// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/vrp_flags/vrp_flags_impl.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "components/vrp_flags/vrp_flags.h"
#include "sandbox/policy/switches.h"

namespace vrp_flags {

namespace {

std::string GetProcessType() {
  std::string type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type");
  if (type.empty()) {
    return "browser";
  }
  return type;
}
}  // namespace

// static
VrpFlagsImpl* VrpFlagsImpl::GetInstance() {
  static base::NoDestructor<VrpFlagsImpl> instance;
  return instance.get();
}

VrpFlagsImpl::VrpFlagsImpl() = default;
VrpFlagsImpl::~VrpFlagsImpl() = default;

void VrpFlagsImpl::Bind(mojo::PendingReceiver<mojom::VrpFlags> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VrpFlagsImpl::GetWriteLocations(GetWriteLocationsCallback callback) {
  EnsureAllocated();
  std::vector<uint64_t> write_locations;
  for (uint64_t* loc : write_locations_) {
    // SAFETY - Cast addresses and pass to renderer as locations to attempt
    // a write. While this is technically a leak of privileged process addresses
    // it is deliberate and only reachable in --vrp-flags mode.
    UNSAFE_BUFFERS(write_locations.push_back(reinterpret_cast<uint64_t>(loc)););
  }
  std::move(callback).Run(write_locations, arbitrary_value_);
}

void VrpFlagsImpl::WriteAttempted(uint64_t location,
                                  WriteAttemptedCallback callback) {
  CHECK(!write_allocations_.empty());
  bool success = false;
  uint32_t index = 0;
  for (uint64_t* loc : write_locations_) {
    if (UNSAFE_BUFFERS(loc == reinterpret_cast<uint64_t*>(location))) {
      if (*loc == arbitrary_value_) {
        LOG(ERROR) << "Security VrpFlags Write Success index=" << index
                   << " location=" << location << " value=" << arbitrary_value_;
        success = true;
      }
      break;
    }
    index++;
  }

  if (!for_testing_) {
    LOG(FATAL) << "Security VrpFlags Write Attempted in " << GetProcessType()
               << " process! location=" << location << " success=" << success;
  }
  std::move(callback).Run(success);
}

void VrpFlagsImpl::GetReadPrefix(GetReadPrefixCallback callback) {
  EnsureAllocated();
  std::move(callback).Run(read_flag_->prefix);
}

void VrpFlagsImpl::ReadAttempted(const base::UnguessableToken& flag,
                                 ReadAttemptedCallback callback) {
  CHECK(read_flag_);
  bool success = (read_flag_ && read_flag_->flag == flag);
  if (!for_testing_) {
    LOG(FATAL) << "Security VrpFlags Read Attempted in " << GetProcessType()
               << " process! success=" << success;
  }
  std::move(callback).Run(success);
}

void VrpFlagsImpl::SetReadValueForTesting(const base::UnguessableToken& flag) {
  EnsureAllocated();
  read_flag_->flag = flag;
}

void VrpFlagsImpl::EnsureAllocated() {
  if (!write_allocations_.empty()) {
    return;
  }

  read_flag_ = std::make_unique<ReadFlag>();
  read_flag_->prefix = base::UnguessableToken::Create();
  read_flag_->flag = base::UnguessableToken::Create();

  arbitrary_value_ = base::RandUint64();
  const size_t kSizes[] = {4096, 16384, 65536, 262144, 1048576};
  for (size_t size : kSizes) {
    auto allocation =
        base::HeapArray<uint64_t>::WithSize(size / sizeof(uint64_t));
    // Pick a random offset that is not at the start.
    size_t offset = base::RandInt(1, (size / sizeof(uint64_t)) - 1);
    // SAFETY - Keep the pointer for later, the allocation outlives uses
    // of the pointer, and this is only reachable in --vrp-flags mode.
    UNSAFE_BUFFERS(uint64_t* address = &allocation.data()[offset]);
    write_locations_.push_back(address);
    write_allocations_.push_back(std::move(allocation));
  }
}

}  // namespace vrp_flags
