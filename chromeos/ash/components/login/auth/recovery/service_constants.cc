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

const char kEpochSuffix[] = "v1/epoch/1";
const char kMediateSuffix[] = "v1/cryptorecovery";

GURL GetRecoveryServiceBaseURL() {
  return GURL(kTestingRecoveryServiceUrl);
}

}  // namespace

std::string GetRecoveryHsmPublicKey() {
  return kTestingHsmPublicKey;
}

GURL GetRecoveryServiceEpochURL() {
  return GetRecoveryServiceBaseURL().Resolve(kEpochSuffix);
}

GURL GetRecoveryServiceMediateURL() {
  return GetRecoveryServiceBaseURL().Resolve(kMediateSuffix);
}

}  // namespace ash
