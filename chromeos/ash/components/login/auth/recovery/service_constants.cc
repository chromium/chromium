// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/service_constants.h"

#include "url/gurl.h"

namespace ash {

namespace {

const char kTestingRecoveryServiceUrl[] =
    "https://autopush-chromeoslogin-pa.sandbox.googleapis.com";

const char kTestingHsmPublicKey[] =
    "3059301306072a8648ce3d020106082a8648ce3d03010703420004240237734dac9e973653"
    "3633dc0de71f926d919927e9190aa409a89ffc8fa8b6072516ddc88785ae78de0411357d27"
    "0b1793859f1d8725911005b4384edcda7f";

// Hard-coded development ledger info, including the public key, name and key
// hash. It mirrors the value from the server.
constexpr char kDevLedgerName[] = "ChromeOSLedgerOwnerPrototype";
constexpr uint32_t kDevLedgerPublicKeyHash = 0x960a3b30;
constexpr char kDevLedgerPublicKey[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEUL2cKW4wHEdyWDjjJktxkijFOKJZ8rflR-Sfb-"
    "ToowJtLyNOBh6wj0anP4kP4llXK4HMZoJDKy9texKJl2UOog==";

const char kEpochSuffix[] = "v1/epoch/1";
const char kMediateSuffix[] = "v1/cryptorecovery";
const char kReauthTokenSuffix[] = "v1/rart";

const char kRecoveryOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromeoscryptorecovery";

GURL GetRecoveryServiceBaseURL() {
  return GURL(kTestingRecoveryServiceUrl);
}

}  // namespace

std::string GetRecoveryHsmPublicKey() {
  return kTestingHsmPublicKey;
}

std::string GetRecoveryLedgerName() {
  return kDevLedgerName;
}

std::string GetRecoveryLedgerPublicKey() {
  return kDevLedgerPublicKey;
}

uint32_t GetRecoveryLedgerPublicKeyHash() {
  return kDevLedgerPublicKeyHash;
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
