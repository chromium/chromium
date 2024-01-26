// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/service_constants.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"

namespace ash {

namespace {

const char kTestingRecoveryServiceUrl[] =
    "https://autopush-chromeoslogin-pa.sandbox.googleapis.com";

const char kRecoveryServiceUrl[] = "https://chromeoslogin-pa.googleapis.com";

const char kTestingHsmPublicKey[] =
    "3059301306072a8648ce3d020106082a8648ce3d03010703420004240237734dac9e973653"
    "3633dc0de71f926d919927e9190aa409a89ffc8fa8b6072516ddc88785ae78de0411357d27"
    "0b1793859f1d8725911005b4384edcda7f";

// -----BEGIN PUBLIC KEY-----
// MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEdFTQOFwWlhLl5fT91iOSRrYBUaBM
// rfy2N2FWRO2BWE1/lAu+kr1H9RhldOZkA89T9dX7pbPRehuueIO8xAPn9A==
// -----END PUBLIC KEY-----
// Converted from Base64 to hex string.
const char kHsmPublicKey[] =
    "3059301306072a8648ce3d020106082a8648ce3d030107034200047454d0385c169612e5e5"
    "f4fdd6239246b60151a04cadfcb637615644ed81584d7f940bbe92bd47f5186574e66403cf"
    "53f5d5fba5b3d17a1bae7883bcc403e7f4";

// Hard-coded ledger info, including the public key, name and key
// hash. It mirrors the value from the server.
constexpr char kDevLedgerName[] = "recoverylog.chromebook.com";
constexpr char kLedgerName[] = "recoverylog.chromebook.com";

constexpr uint32_t kDevLedgerPublicKeyHash = 0x960a3b30;
constexpr uint32_t kLedgerPublicKeyHash = 0x5a30b187;

constexpr char kDevLedgerPublicKey[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEUL2cKW4wHEdyWDjjJktxkijFOKJZ8rflR-Sfb-"
    "ToowJtLyNOBh6wj0anP4kP4llXK4HMZoJDKy9texKJl2UOog==";
constexpr char kLedgerPublicKey[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEW_0IGDyIKy_lA10CIjNV4dy3G1jVLIhabzRLJJ"
    "DSD9nesZLv6Pqe0MVRGjncQkCjh4lOwOsOwMbdRaux8R912w==";

const char kEpochSuffix[] = "v1/epoch/1";
const char kMediateSuffix[] = "v1/cryptorecovery";
const char kReauthTokenSuffix[] = "v1/rart";

const char kRecoveryOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromeoscryptorecovery";

bool IsUsingTestEnvironment() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      ash::switches::kCryptohomeRecoveryUseTestEnvironment);
}

GURL GetRecoveryServiceBaseURL() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCryptohomeRecoveryServiceBaseUrl)) {
    return GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kCryptohomeRecoveryServiceBaseUrl));
  }
  if (IsUsingTestEnvironment()) {
    return GURL(kTestingRecoveryServiceUrl);
  }
  return GURL(kRecoveryServiceUrl);
}

}  // namespace

std::string GetRecoveryHsmPublicKey() {
  std::string decoded_key;
  const bool result = base::HexStringToString(
      IsUsingTestEnvironment() ? kTestingHsmPublicKey : kHsmPublicKey,
      &decoded_key);
  CHECK(result);
  return decoded_key;
}

std::string GetRecoveryLedgerName() {
  if (IsUsingTestEnvironment()) {
    return kDevLedgerName;
  }
  return kLedgerName;
}

std::string GetRecoveryLedgerPublicKey() {
  if (IsUsingTestEnvironment()) {
    return kDevLedgerPublicKey;
  }
  return kLedgerPublicKey;
}

uint32_t GetRecoveryLedgerPublicKeyHash() {
  if (IsUsingTestEnvironment()) {
    return kDevLedgerPublicKeyHash;
  }
  return kLedgerPublicKeyHash;
}

GURL GetRecoveryServiceEpochURL() {
  return GetRecoveryServiceBaseURL().Resolve(kEpochSuffix);
}

GURL GetRecoveryServiceMediateURL() {
  return GetRecoveryServiceBaseURL().Resolve(kMediateSuffix);
}

GURL GetRecoveryServiceReauthTokenURL() {
  return GetRecoveryServiceBaseURL().Resolve(kReauthTokenSuffix);
}

std::vector<std::string> GetRecoveryOAuth2Scope() {
  return {kRecoveryOAuth2Scope};
}

}  // namespace ash
