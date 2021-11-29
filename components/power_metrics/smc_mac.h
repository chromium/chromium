// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The System Management Controller (SMC) is a hardware component that controls
// the power functions of Intel-based Macs. This file defines a class to read
// known SMC keys.

#ifndef COMPONENTS_POWER_METRICS_SMC_MAC_H_
#define COMPONENTS_POWER_METRICS_SMC_MAC_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/mac/scoped_ioobject.h"
#include "components/power_metrics/smc_internal_types_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace power_metrics {

class SMCReader {
 public:
  // Creates an SMC Reader. Returns nullptr in case of failure.
  static std::unique_ptr<SMCReader> Create();

  virtual ~SMCReader();

  // Returns the power consumption of various hardware components in watts.
  // Virtual for testing.
  virtual absl::optional<double> ReadTotalPowerW();
  virtual absl::optional<double> ReadCPUPackageCPUPowerW();
  virtual absl::optional<double> ReadCPUPackageGPUPowerW();
  virtual absl::optional<double> ReadGPU0PowerW();
  virtual absl::optional<double> ReadGPU1PowerW();

 protected:
  explicit SMCReader(base::mac::ScopedIOObject<io_object_t> connect);

 private:
  class SMCKey {
   public:
    SMCKey(base::mac::ScopedIOObject<io_object_t> connect,
           SMCKeyIdentifier key_identifier);
    ~SMCKey();

    bool Exists() const;
    absl::optional<double> Read();

   private:
    bool CallSMCFunction(uint8_t function, SMCParamStruct* out);

    base::mac::ScopedIOObject<io_object_t> connect_;
    const SMCKeyIdentifier key_identifier_;
    SMCKeyInfoData key_info_;
  };

  SMCKey total_power_key_;
  SMCKey cpu_package_cpu_power_key_;
  SMCKey cpu_package_gpu_power_key_;
  SMCKey gpu0_power_key_;
  SMCKey gpu1_power_key_;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_SMC_MAC_H_
