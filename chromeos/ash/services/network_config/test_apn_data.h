// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_TEST_APN_DATA_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_TEST_APN_DATA_H_

#include <string>
#include <vector>

#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash::network_config {

// TestApnData helps test creating APNs in different formats from a single data
// source. This data source should contain all the expected APN property fields
// required for a test. This class also helps comparing the an APN's properties
// with the test data source.
struct TestApnData {
  // Empty constructor to let the test set the required fields.
  TestApnData();

  // Explicit constructor to initialize an APN with all its fields.
  TestApnData(std::string access_point_name,
              std::string name,
              std::string username,
              std::string password,
              std::string attach,
              std::string id,
              chromeos::network_config::mojom::ApnState mojo_state,
              std::string onc_state,
              chromeos::network_config::mojom::ApnAuthenticationType
                  mojo_authentication,
              std::string onc_authentication,
              chromeos::network_config::mojom::ApnIpType mojo_ip_type,
              std::string onc_ip_type,
              chromeos::network_config::mojom::ApnSource mojo_source,
              std::string onc_source,
              const std::vector<chromeos::network_config::mojom::ApnType>&
                  mojo_apn_types,
              const std::vector<std::string>& onc_apn_types);

  ~TestApnData();

  std::string access_point_name;
  std::string name;
  std::string username;
  std::string password;
  std::string attach;
  std::string id;

  chromeos::network_config::mojom::ApnState mojo_state;
  std::string onc_state;

  chromeos::network_config::mojom::ApnAuthenticationType mojo_authentication;
  std::string onc_authentication;

  chromeos::network_config::mojom::ApnIpType mojo_ip_type;
  std::string onc_ip_type;

  chromeos::network_config::mojom::ApnSource mojo_source;
  std::string onc_source;

  std::vector<chromeos::network_config::mojom::ApnType> mojo_apn_types;
  std::vector<std::string> onc_apn_types;

  chromeos::network_config::mojom::ApnPropertiesPtr AsMojoApn() const;
  base::Value::Dict AsOncApn() const;
  base::Value::Dict AsShillApn() const;
  std::string AsApnShillDict() const;

  // Verifies that an APN constructed as a Mojo struct matches with the test
  // APN data.
  bool MojoApnEquals(
      const chromeos::network_config::mojom::ApnProperties& apn) const;

  // Verifies that an APN constructed as an ONC dictionary matches with the
  // test APN data.
  bool OncApnEquals(const base::Value::Dict& onc_apn,
                    bool has_state_field,
                    bool is_password_masked) const;
};

}  // namespace ash::network_config

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_TEST_APN_DATA_H_
