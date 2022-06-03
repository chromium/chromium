// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_normalizer.h"

#include <memory>

#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

// Validate that StaticIPConfig IPAddress and dependent fields will be removed
// if IPAddressConfigType is not 'Static'.
TEST(ONCNormalizerTest, RemoveUnnecessaryAddressStaticIPConfigFields) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("unnecessary-address-staticipconfig", &original);
  data->GetDictionary("unnecessary-address-staticipconfig-normalized",
                      &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

// Validate that StaticIPConfig fields other than NameServers and IPAddress &
// friends will be retained even without static
// {NameServers,IPAddress}ConfigType.
TEST(ONCNormalizerTest, RetainExtraStaticIPConfigFields) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("unnecessary-address-staticipconfig", &original);
  data->GetDictionary("unnecessary-address-staticipconfig-normalized",
                      &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

// Validate that irrelevant fields of the StaticIPConfig dictionary will be
// removed.
TEST(ONCNormalizerTest, RemoveStaticIPConfigFields) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("irrelevant-staticipconfig-fields", &original);
  data->GetDictionary("irrelevant-staticipconfig-fields-normalized",
                      &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

// Validate that StatticIPConfig.NameServers is removed when
// NameServersConfigType is 'DHCP'.
TEST(ONCNormalizerTest, RemoveNameServers) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("irrelevant-nameservers", &original);
  data->GetDictionary("irrelevant-nameservers-normalized",
                      &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

// Validate that IPConfig related fields are removed when IPAddressConfigType
// is 'Static', but some required fields are missing.
TEST(ONCNormalizerTest, RemoveIPFieldsForIncompleteConfig) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("missing-ip-fields", &original);
  data->GetDictionary("missing-ip-fields-normalized", &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}
// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfigurationEthernetAndVPN) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("ethernet-and-vpn", &original);
  data->GetDictionary("ethernet-and-vpn-normalized", &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfigurationWifi) {
  Normalizer normalizer(true);
  std::unique_ptr<const base::DictionaryValue> data(
      test_utils::ReadTestDictionary("settings_with_normalization.json"));

  const base::DictionaryValue* original = nullptr;
  const base::DictionaryValue* expected_normalized = nullptr;
  data->GetDictionary("wifi", &original);
  data->GetDictionary("wifi-normalized", &expected_normalized);

  std::unique_ptr<base::DictionaryValue> actual_normalized =
      normalizer.NormalizeObject(&kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, actual_normalized.get()));
}

}  // namespace onc
}  // namespace chromeos
