// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_SMC_INTERNAL_TYPES_MAC_H_
#define COMPONENTS_POWER_METRICS_SMC_INTERNAL_TYPES_MAC_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

// List of known SMC key identifiers.
//
// This is a good reference: https://logi.wiki/index.php/SMC_Sensor_Codes
// Additional keys can be discovered with
// https://github.com/theopolis/smc-fuzzer
enum class SMCKeyIdentifier : uint32_t {
  TotalPower = 'PSTR',      // Power: System Total Rail (watts)
  CPUPower = 'PCPC',        // Power: CPU Package CPU (watts)
  iGPUPower = 'PCPG',       // Power: CPU Package GPU (watts)
  GPU0Power = 'PG0R',       // Power: GPU 0 Rail (watts)
  GPU1Power = 'PG1R',       // Power: GPU 1 Rail (watts)
  CPUTemperature = 'TC0F',  // Temperature: CPU Die PECI (Celsius)
};

// Types from PowerManagement/pmconfigd/PrivateLib.c
// (https://opensource.apple.com/source/PowerManagement/PowerManagement-494.1.2/pmconfigd/PrivateLib.c.auto.html)
struct SMCVersion {
  unsigned char major;
  unsigned char minor;
  unsigned char build;
  unsigned char reserved;
  unsigned short release;
};

struct SMCPLimitData {
  uint16_t version;
  uint16_t length;
  uint32_t cpuPLimit;
  uint32_t gpuPLimit;
  uint32_t memPLimit;
};

enum class SMCDataType : uint32_t {
  flt = 'flt ',   // Floating point
  sp78 = 'sp78',  // Fixed point: SIIIIIIIFFFFFFFF
  sp87 = 'sp87',  // Fixed point: SIIIIIIIIFFFFFFF
  spa5 = 'spa5',  // Fixed point: SIIIIIIIIIIFFFFF
};

struct SMCKeyInfoData {
  IOByteCount dataSize;
  SMCDataType dataType;
  uint8_t dataAttributes;
};

struct SMCParamStruct {
  SMCKeyIdentifier key;
  SMCVersion vers;
  SMCPLimitData pLimitData;
  SMCKeyInfoData keyInfo;
  uint8_t result;
  uint8_t status;
  uint8_t data8;
  uint32_t data32;
  uint8_t bytes[32];
};

enum {
  kSMCUserClientOpen = 0,
  kSMCUserClientClose = 1,
  kSMCHandleYPCEvent = 2,
  kSMCReadKey = 5,
  kSMCWriteKey = 6,
  kSMCGetKeyCount = 7,
  kSMCGetKeyFromIndex = 8,
  kSMCGetKeyInfo = 9
};

#endif  // COMPONENTS_POWER_METRICS_SMC_INTERNAL_TYPES_MAC_H_
