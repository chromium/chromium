// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_BEACON_SEED_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_BEACON_SEED_H_

#include <google/protobuf/repeated_field.h>
#include <ostream>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"

namespace chromeos {

namespace multidevice {

// Salt value used to generate ephemeral IDs for bootstrapping connections.
// A BeaconSeed value is valid only between its start and end timestamps.
//
// This class should always be preferred over the cryptauth::BeaconSeed proto
// except when communicating with the CryptAuth server.
class BeaconSeed {
 public:
  BeaconSeed();
  BeaconSeed(const std::string& data,
             base::Time start_time,
             base::Time end_time);
  BeaconSeed(const BeaconSeed& other);
  ~BeaconSeed();

  const std::string& data() const { return data_; }
  base::Time start_time() const { return start_time_; }
  base::Time end_time() const { return end_time_; }

  bool operator==(const BeaconSeed& other) const;

 private:
  std::string data_;
  base::Time start_time_;
  base::Time end_time_;
};

BeaconSeed FromCryptAuthSeed(cryptauth::BeaconSeed cryptauth_seed);
cryptauth::BeaconSeed ToCryptAuthSeed(BeaconSeed multidevice_seed);

BeaconSeed FromCryptAuthV2Seed(cryptauthv2::BeaconSeed cryptauth_seed);
cryptauthv2::BeaconSeed ToCryptAuthV2Seed(BeaconSeed multidevice_seed);

std::vector<cryptauth::BeaconSeed> ToCryptAuthSeedList(
    const std::vector<BeaconSeed>& cryptauth_seed_list);
std::vector<BeaconSeed> FromCryptAuthSeedList(
    const std::vector<cryptauth::BeaconSeed>& cryptauth_seed_list);

std::vector<BeaconSeed> FromCryptAuthV2SeedRepeatedPtrField(
    const google::protobuf::RepeatedPtrField<cryptauthv2::BeaconSeed>&
        cryptauth_seed_list);

std::ostream& operator<<(std::ostream& stream, const BeaconSeed& beacon_seed);

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_BEACON_SEED_H_
