// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_normalizer.h"

#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::onc {

namespace test_utils = ::chromeos::onc::test_utils;

// Validate that StaticIPConfig IPAddress and dependent fields will be removed
// if IPAddressConfigType is not 'Static'.
TEST(ONCNormalizerTest, RemoveUnnecessaryAddressStaticIPConfigFields) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original =
      data.FindDict("unnecessary-address-staticipconfig");
  const base::Value::Dict* expected_normalized =
      data.FindDict("unnecessary-address-staticipconfig-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that StaticIPConfig fields other than NameServers and IPAddress &
// friends will be retained even without static
// {NameServers,IPAddress}ConfigType.
TEST(ONCNormalizerTest, RetainExtraStaticIPConfigFields) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original =
      data.FindDict("unnecessary-address-staticipconfig");
  const base::Value::Dict* expected_normalized =
      data.FindDict("unnecessary-address-staticipconfig-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that irrelevant fields of the StaticIPConfig dictionary will be
// removed.
TEST(ONCNormalizerTest, RemoveStaticIPConfigFields) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original =
      data.FindDict("irrelevant-staticipconfig-fields");
  const base::Value::Dict* expected_normalized =
      data.FindDict("irrelevant-staticipconfig-fields-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that StatticIPConfig.NameServers is removed when
// NameServersConfigType is 'DHCP'.
TEST(ONCNormalizerTest, RemoveNameServers) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original = data.FindDict("irrelevant-nameservers");
  const base::Value::Dict* expected_normalized =
      data.FindDict("irrelevant-nameservers-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// Validate that IPConfig related fields are removed when IPAddressConfigType
// is 'Static', but some required fields are missing.
TEST(ONCNormalizerTest, RemoveIPFieldsForIncompleteConfig) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original = data.FindDict("missing-ip-fields");
  const base::Value::Dict* expected_normalized =
      data.FindDict("missing-ip-fields-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}
// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfigurationEthernetAndVPN) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original = data.FindDict("ethernet-and-vpn");
  const base::Value::Dict* expected_normalized =
      data.FindDict("ethernet-and-vpn-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

// This test case is about validating valid ONC objects.
TEST(ONCNormalizerTest, NormalizeNetworkConfigurationWifi) {
  Normalizer normalizer(true);
  base::Value::Dict data =
      test_utils::ReadTestDictionary("settings_with_normalization.json");

  const base::Value::Dict* original = data.FindDict("wifi");
  const base::Value::Dict* expected_normalized =
      data.FindDict("wifi-normalized");

  base::Value::Dict actual_normalized = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, *original);
  EXPECT_TRUE(test_utils::Equals(expected_normalized, &actual_normalized));
}

}  // namespace ash::onc
