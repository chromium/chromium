// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/smc_mac.h"

#include <libkern/OSByteOrder.h>
#include <utility>

#include "base/memory/ptr_util.h"

namespace power_metrics {

namespace {

double FromSMCFixedPoint(uint8_t* bytes, size_t fraction_bits) {
  return OSReadBigInt16(bytes, 0) / static_cast<double>(1 << fraction_bits);
}

}  // namespace

// static
std::unique_ptr<SMCReader> SMCReader::Create() {
  const base::mac::ScopedIOObject<io_service_t> smc_service(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("AppleSMC")));
  base::mac::ScopedIOObject<io_object_t> connect;
  if (IOServiceOpen(smc_service.get(), mach_task_self(), 1,
                    connect.InitializeInto()) != kIOReturnSuccess) {
    return nullptr;
  }

  return base::WrapUnique(new SMCReader(std::move(connect)));
}

SMCReader::~SMCReader() = default;

std::optional<double> SMCReader::ReadKey(SMCKeyIdentifier identifier) {
  auto it = keys_.find(identifier);
  if (it == keys_.end()) {
    auto result = keys_.emplace(identifier, SMCKey(connect_, identifier));
    it = result.first;
  }

  return it->second.Read();
}

SMCReader::SMCKey::SMCKey(base::mac::ScopedIOObject<io_object_t> connect,
                          SMCKeyIdentifier key_identifier)
    : connect_(std::move(connect)), key_identifier_(key_identifier) {
  // Read key information.
  SMCParamStruct out{};
  if (CallSMCFunction(kSMCGetKeyInfo, &out))
    key_info_ = out.keyInfo;
}

SMCReader::SMCKey::SMCKey(SMCKey&&) = default;
SMCReader::SMCKey& SMCReader::SMCKey::operator=(SMCKey&&) = default;

SMCReader::SMCKey::~SMCKey() = default;

bool SMCReader::SMCKey::Exists() const {
  return key_info_.dataSize > 0;
}

std::optional<double> SMCReader::SMCKey::Read() {
  if (!Exists())
    return std::nullopt;

  SMCParamStruct out{};
  if (!CallSMCFunction(kSMCReadKey, &out))
    return std::nullopt;
  switch (key_info_.dataType) {
    case SMCDataType::flt:
      return *reinterpret_cast<float*>(out.bytes);
    case SMCDataType::sp78:
      return FromSMCFixedPoint(out.bytes, 8);
    case SMCDataType::sp87:
      return FromSMCFixedPoint(out.bytes, 7);
    case SMCDataType::spa5:
      return FromSMCFixedPoint(out.bytes, 5);
    default:
      return std::nullopt;
  }
}

bool SMCReader::SMCKey::CallSMCFunction(uint8_t function, SMCParamStruct* out) {
  if (!connect_)
    return false;

  // TODO: In local tests, removing the calls to `kSMCUserClientOpen` and
  // `kSMCUserClientClose` doesn't seem to affect behavior. Consider removing
  // them.

  if (IOConnectCallMethod(connect_.get(), kSMCUserClientOpen, nullptr, 0,
                          nullptr, 0, nullptr, nullptr, nullptr, nullptr)) {
    connect_.reset();
    return false;
  }

  SMCParamStruct in{};
  in.key = key_identifier_;
  in.keyInfo.dataSize = key_info_.dataSize;
  in.data8 = function;

  size_t out_size = sizeof(*out);
  const bool success =
      IOConnectCallStructMethod(connect_.get(), kSMCHandleYPCEvent, &in,
                                sizeof(in), out, &out_size) == kIOReturnSuccess;

  if (IOConnectCallMethod(connect_.get(), kSMCUserClientClose, nullptr, 0,
                          nullptr, 0, nullptr, nullptr, nullptr, nullptr)) {
    connect_.reset();
  }

  // Even if the close failed, report whether the actual call succeeded.
  return success;
}

SMCReader::SMCReader(base::mac::ScopedIOObject<io_object_t> connect)
    : connect_(std::move(connect)) {}

}  // namespace power_metrics
