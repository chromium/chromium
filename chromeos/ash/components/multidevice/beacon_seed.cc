// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/beacon_seed.h"

#include "base/base64.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"

namespace ash::multidevice {

BeaconSeed::BeaconSeed() = default;

BeaconSeed::BeaconSeed(const std::string& data,
                       base::Time start_time,
                       base::Time end_time)
    : data_(data), start_time_(start_time), end_time_(end_time) {}

BeaconSeed::BeaconSeed(const BeaconSeed& other) = default;

BeaconSeed::~BeaconSeed() = default;

bool BeaconSeed::operator==(const BeaconSeed& other) const {
  return data_ == other.data_ && start_time_ == other.start_time_ &&
         end_time_ == other.end_time_;
}

BeaconSeed FromCryptAuthSeed(cryptauth::BeaconSeed cryptauth_seed) {
  return BeaconSeed(cryptauth_seed.data(),
                    base::Time::FromMillisecondsSinceUnixEpoch(
                        cryptauth_seed.start_time_millis()),
                    base::Time::FromMillisecondsSinceUnixEpoch(
                        cryptauth_seed.end_time_millis()));
}

cryptauth::BeaconSeed ToCryptAuthSeed(BeaconSeed multidevice_seed) {
  cryptauth::BeaconSeed cryptauth_seed;
  cryptauth_seed.set_data(multidevice_seed.data());
  cryptauth_seed.set_start_time_millis(
      multidevice_seed.start_time().InMillisecondsSinceUnixEpoch());
  cryptauth_seed.set_end_time_millis(
      multidevice_seed.end_time().InMillisecondsSinceUnixEpoch());
  return cryptauth_seed;
}

BeaconSeed FromCryptAuthV2Seed(cryptauthv2::BeaconSeed cryptauth_seed) {
  return BeaconSeed(cryptauth_seed.data(),
                    base::Time::FromMillisecondsSinceUnixEpoch(
                        cryptauth_seed.start_time_millis()),
                    base::Time::FromMillisecondsSinceUnixEpoch(
                        cryptauth_seed.end_time_millis()));
}

cryptauthv2::BeaconSeed ToCryptAuthV2Seed(BeaconSeed multidevice_seed) {
  cryptauthv2::BeaconSeed cryptauth_seed;
  cryptauth_seed.set_data(multidevice_seed.data());
  cryptauth_seed.set_start_time_millis(
      multidevice_seed.start_time().InMillisecondsSinceUnixEpoch());
  cryptauth_seed.set_end_time_millis(
      multidevice_seed.end_time().InMillisecondsSinceUnixEpoch());
  return cryptauth_seed;
}

std::vector<cryptauth::BeaconSeed> ToCryptAuthSeedList(
    const std::vector<BeaconSeed>& multidevice_seed_list) {
  std::vector<cryptauth::BeaconSeed> cryptauth_beacon_seeds;
  base::ranges::transform(multidevice_seed_list,
                          std::back_inserter(cryptauth_beacon_seeds),
                          &ToCryptAuthSeed);
  return cryptauth_beacon_seeds;
}

std::vector<BeaconSeed> FromCryptAuthSeedList(
    const std::vector<cryptauth::BeaconSeed>& cryptauth_seed_list) {
  std::vector<BeaconSeed> multidevice_beacon_seeds;
  base::ranges::transform(cryptauth_seed_list,
                          std::back_inserter(multidevice_beacon_seeds),
                          &FromCryptAuthSeed);
  return multidevice_beacon_seeds;
}

std::vector<BeaconSeed> FromCryptAuthV2SeedRepeatedPtrField(
    const google::protobuf::RepeatedPtrField<cryptauthv2::BeaconSeed>&
        cryptauth_seed_list) {
  std::vector<BeaconSeed> multidevice_beacon_seeds;
  base::ranges::transform(cryptauth_seed_list,
                          std::back_inserter(multidevice_beacon_seeds),
                          &FromCryptAuthV2Seed);
  return multidevice_beacon_seeds;
}

std::ostream& operator<<(std::ostream& stream, const BeaconSeed& beacon_seed) {
  std::string base_64_data = base::Base64Encode(beacon_seed.data());

  stream << "{base_64_data: \"" << base_64_data << "\", start_time: \""
         << base::TimeFormatShortDateAndTimeWithTimeZone(
                beacon_seed.start_time())
         << "\", "
         << "end_time: \""
         << base::TimeFormatShortDateAndTimeWithTimeZone(beacon_seed.end_time())
         << "\"}";

  return stream;
}

}  // namespace ash::multidevice
