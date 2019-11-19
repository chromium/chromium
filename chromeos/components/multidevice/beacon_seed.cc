// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/beacon_seed.h"

#include <algorithm>

#include "base/base64.h"
#include "base/i18n/time_formatting.h"

namespace chromeos {

namespace multidevice {

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
  return BeaconSeed(
      cryptauth_seed.data(),
      base::Time::FromJavaTime(cryptauth_seed.start_time_millis()),
      base::Time::FromJavaTime(cryptauth_seed.end_time_millis()));
}

cryptauth::BeaconSeed ToCryptAuthSeed(BeaconSeed multidevice_seed) {
  cryptauth::BeaconSeed cryptauth_seed;
  cryptauth_seed.set_data(multidevice_seed.data());
  cryptauth_seed.set_start_time_millis(
      multidevice_seed.start_time().ToJavaTime());
  cryptauth_seed.set_end_time_millis(multidevice_seed.end_time().ToJavaTime());
  return cryptauth_seed;
}

BeaconSeed FromCryptAuthV2Seed(cryptauthv2::BeaconSeed cryptauth_seed) {
  return BeaconSeed(
      cryptauth_seed.data(),
      base::Time::FromJavaTime(cryptauth_seed.start_time_millis()),
      base::Time::FromJavaTime(cryptauth_seed.end_time_millis()));
}

cryptauthv2::BeaconSeed ToCryptAuthV2Seed(BeaconSeed multidevice_seed) {
  cryptauthv2::BeaconSeed cryptauth_seed;
  cryptauth_seed.set_data(multidevice_seed.data());
  cryptauth_seed.set_start_time_millis(
      multidevice_seed.start_time().ToJavaTime());
  cryptauth_seed.set_end_time_millis(multidevice_seed.end_time().ToJavaTime());
  return cryptauth_seed;
}

std::vector<cryptauth::BeaconSeed> ToCryptAuthSeedList(
    const std::vector<BeaconSeed>& multidevice_seed_list) {
  std::vector<cryptauth::BeaconSeed> cryptauth_beacon_seeds;
  std::transform(multidevice_seed_list.begin(), multidevice_seed_list.end(),
                 std::back_inserter(cryptauth_beacon_seeds),
                 [](auto multidevice_beacon_seed) {
                   return ToCryptAuthSeed(multidevice_beacon_seed);
                 });
  return cryptauth_beacon_seeds;
}

std::vector<BeaconSeed> FromCryptAuthSeedList(
    const std::vector<cryptauth::BeaconSeed>& cryptauth_seed_list) {
  std::vector<BeaconSeed> multidevice_beacon_seeds;
  std::transform(cryptauth_seed_list.begin(), cryptauth_seed_list.end(),
                 std::back_inserter(multidevice_beacon_seeds),
                 [](auto cryptauth_beacon_seed) {
                   return FromCryptAuthSeed(cryptauth_beacon_seed);
                 });
  return multidevice_beacon_seeds;
}

std::vector<BeaconSeed> FromCryptAuthV2SeedRepeatedPtrField(
    const google::protobuf::RepeatedPtrField<cryptauthv2::BeaconSeed>&
        cryptauth_seed_list) {
  std::vector<BeaconSeed> multidevice_beacon_seeds;
  std::transform(cryptauth_seed_list.begin(), cryptauth_seed_list.end(),
                 std::back_inserter(multidevice_beacon_seeds),
                 [](auto cryptauth_beacon_seed) {
                   return FromCryptAuthV2Seed(cryptauth_beacon_seed);
                 });
  return multidevice_beacon_seeds;
}

std::ostream& operator<<(std::ostream& stream, const BeaconSeed& beacon_seed) {
  std::string base_64_data;
  base::Base64Encode(beacon_seed.data(), &base_64_data);

  stream << "{base_64_data: \"" << base_64_data << "\", start_time: \""
         << base::TimeFormatShortDateAndTime(beacon_seed.start_time()) << "\", "
         << "end_time: \""
         << base::TimeFormatShortDateAndTime(beacon_seed.start_time()) << "\"}";

  return stream;
}

}  // namespace multidevice

}  // namespace chromeos
