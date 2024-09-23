// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The System Management Controller (SMC) is a hardware component that controls
// the power functions of Intel-based Macs. This file defines a class to read
// known SMC keys.

#ifndef COMPONENTS_POWER_METRICS_SMC_MAC_H_
#define COMPONENTS_POWER_METRICS_SMC_MAC_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/mac/scoped_ioobject.h"
#include "components/power_metrics/smc_internal_types_mac.h"

namespace power_metrics {

class SMCReader {
 public:
  // Creates an SMC Reader. Returns nullptr in case of failure.
  static std::unique_ptr<SMCReader> Create();

  virtual ~SMCReader();

  // Returns the value of a key, or nullopt if not available.
  // Virtual for testing.
  virtual std::optional<double> ReadKey(SMCKeyIdentifier identifier);

 protected:
  explicit SMCReader(base::mac::ScopedIOObject<io_object_t> connect);

 private:
  class SMCKey {
   public:
    SMCKey(base::mac::ScopedIOObject<io_object_t> connect,
           SMCKeyIdentifier key_identifier);
    SMCKey(SMCKey&&);
    SMCKey& operator=(SMCKey&&);
    ~SMCKey();

    bool Exists() const;
    std::optional<double> Read();

   private:
    bool CallSMCFunction(uint8_t function, SMCParamStruct* out);

    base::mac::ScopedIOObject<io_object_t> connect_;
    SMCKeyIdentifier key_identifier_;
    SMCKeyInfoData key_info_;
  };

  base::mac::ScopedIOObject<io_object_t> connect_;
  base::flat_map<SMCKeyIdentifier, SMCKey> keys_;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_SMC_MAC_H_
