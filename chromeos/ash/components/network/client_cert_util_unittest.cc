// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/client_cert_util.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::client_cert {

TEST(ClientCertUtilTest, GetPkcs11AndSlotIdFromEapCertId_Ok) {
  int slot_id = -1000;
  EXPECT_EQ(GetPkcs11AndSlotIdFromEapCertId("2:abcd1234", &slot_id),
            "abcd1234");
  EXPECT_EQ(slot_id, 2);
}

TEST(ClientCertUtilTest, GetPkcs11AndSlotIdFromEapCertId_WrongFormat) {
  int slot_id = -1000;
  EXPECT_EQ(GetPkcs11AndSlotIdFromEapCertId("", &slot_id), std::string());
  EXPECT_EQ(slot_id, -1);
}

TEST(ClientCertUtilTest, GetPkcs11AndSlotIdFromEapCertId_EmptyPkcs11Id) {
  int slot_id = -1000;
  EXPECT_EQ(GetPkcs11AndSlotIdFromEapCertId("2:", &slot_id), std::string());
  EXPECT_EQ(slot_id, -1);
}

TEST(ClientCertUtilTest, GetPkcs11AndSlotIdFromEapCertId_BadSlotId) {
  int slot_id = -1000;
  EXPECT_EQ(GetPkcs11AndSlotIdFromEapCertId("a:abcd1234", &slot_id),
            "abcd1234");
  EXPECT_EQ(slot_id, -1);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_OpenVPN) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "OpenVPN.Pkcs11.ID": "abcd1234"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kOpenVpn);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, -1);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_L2TPIPsec) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "L2TPIPsec.ClientCertID": "abcd1234",
           "L2TPIPsec.ClientCertSlot": "2"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kL2tpIpsec);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, 2);
}

TEST(ClientCertUtilTest,
     GetClientCertFromShillProperties_L2TPIPsec_MissingSlot) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "L2TPIPsec.ClientCertID": "abcd1234"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kL2tpIpsec);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, -1);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_L2TPIPsec_EmptySlot) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "L2TPIPsec.ClientCertID": "abcd1234",
           "L2TPIPsec.ClientCertSlot": ""
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kL2tpIpsec);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, -1);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_L2TPIPsec_BadSlot) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "L2TPIPsec.ClientCertID": "abcd1234",
           "L2TPIPsec.ClientCertSlot": "not_an_integer"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kNone);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_IKEv2) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "IKEv2.ClientCertID": "abcd1234",
           "IKEv2.ClientCertSlot": "2"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kIkev2);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, 2);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_IKEv2_MissingSlot) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "IKEv2.ClientCertID": "abcd1234"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kIkev2);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, -1);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_IKEv2_EmptySlot) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "IKEv2.ClientCertID": "abcd1234",
           "IKEv2.ClientCertSlot": ""
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kIkev2);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, -1);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_IKEv2_BadSlot) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "Provider": {
           "IKEv2.ClientCertID": "abcd1234",
           "IKEv2.ClientCertSlot": "not_an_integer"
         }
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kNone);
}

TEST(ClientCertUtilTest, GetClientCertFromShillProperties_EAP) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "EAP.CertID": "2:abcd1234",
         "EAP.KeyID": "2:abcd1234"
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kEap);
  EXPECT_EQ(pkcs11_id, "abcd1234");
  EXPECT_EQ(tpm_slot, 2);
}

TEST(ClientCertUtilTest,
     GetClientCertFromShillProperties_EAP_KeyCertIdMismatch) {
  ConfigType cert_config_type = ConfigType::kNone;
  int tpm_slot = -1;
  std::string pkcs11_id;

  base::Value::Dict shill_properties = base::test::ParseJsonDict(
      R"({
         "EAP.CertID": "2:abcd1234",
         "EAP.KeyID": "3:edfg5678"
       })");
  GetClientCertFromShillProperties(shill_properties, &cert_config_type,
                                   &tpm_slot, &pkcs11_id);
  EXPECT_EQ(cert_config_type, ConfigType::kNone);
}

TEST(ClientCertUtilTest, SetShillProperties_OpenVPN) {
  base::Value::Dict shill_properties;
  SetShillProperties(ConfigType::kOpenVpn, 2, "abcd1234", shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "OpenVPN.Pkcs11.PIN": "111111",
         "OpenVPN.Pkcs11.ID": "abcd1234"
       })"));
}

TEST(ClientCertUtilTest, SetShillProperties_L2TPIPsec) {
  base::Value::Dict shill_properties;
  SetShillProperties(ConfigType::kL2tpIpsec, 2, "abcd1234", shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "L2TPIPsec.PIN": "111111",
         "L2TPIPsec.ClientCertID": "abcd1234",
         "L2TPIPsec.ClientCertSlot": "2"
       })"));
}

TEST(ClientCertUtilTest, SetShillProperties_IKEv2) {
  base::Value::Dict shill_properties;
  SetShillProperties(ConfigType::kIkev2, 2, "abcd1234", shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "IKEv2.ClientCertID": "abcd1234",
         "IKEv2.ClientCertSlot": "2"
       })"));
}

TEST(ClientCertUtilTest, SetShillProperties_EAP) {
  base::Value::Dict shill_properties;
  SetShillProperties(ConfigType::kEap, 2, "abcd1234", shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "EAP.PIN": "111111",
         "EAP.CertID": "2:abcd1234",
         "EAP.KeyID": "2:abcd1234"
       })"));
}

TEST(ClientCertUtilTest, SetEmptyShillProperties_OpenVPN) {
  base::Value::Dict shill_properties;
  SetEmptyShillProperties(ConfigType::kOpenVpn, shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "OpenVPN.Pkcs11.PIN": "",
         "OpenVPN.Pkcs11.ID": ""
       })"));
}

TEST(ClientCertUtilTest, SetEmptyShillProperties_L2TPIPsec) {
  base::Value::Dict shill_properties;
  SetEmptyShillProperties(ConfigType::kL2tpIpsec, shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "L2TPIPsec.PIN": "",
         "L2TPIPsec.ClientCertID": "",
         "L2TPIPsec.ClientCertSlot": ""
       })"));
}

TEST(ClientCertUtilTest, SetEmptyShillProperties_IKEv2) {
  base::Value::Dict shill_properties;
  SetEmptyShillProperties(ConfigType::kIkev2, shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "IKEv2.ClientCertID": "",
         "IKEv2.ClientCertSlot": ""
       })"));
}

TEST(ClientCertUtilTest, SetEmptyShillProperties_EAP) {
  base::Value::Dict shill_properties;
  SetEmptyShillProperties(ConfigType::kEap, shill_properties);

  EXPECT_THAT(shill_properties, base::test::IsJson(R"({
         "EAP.PIN": "",
         "EAP.CertID": "",
         "EAP.KeyID": ""
       })"));
}

TEST(ClientCertUtilTest, OncToClientCertConfig_Empty) {
  base::Value::Dict network_config;
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_NONE, network_config, &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kNone);
}

// The actual tests for various client cert config types are implemented for
// wifi and not repeated for other client cert config locations (such as
// Ethernet, OpenVPN, ...) because the client cert config fields are the same.

TEST(ClientCertUtilTest, OncToClientCertConfig_Wifi_Pattern) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "WiFi",
         "WiFi": {
           "EAP": {
             "Identity": "identity_value",
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Issuer": {
                 "CommonName": "common_name_value"
               }
             }
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kEap);
  EXPECT_EQ(cert_config.client_cert_type, ::onc::client_cert::kPattern);
  EXPECT_EQ(cert_config.guid, std::string());
  EXPECT_EQ(cert_config.provisioning_profile_id, std::string());
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

TEST(ClientCertUtilTest, OncToClientCertConfig_Wifi_Ref) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "WiFi",
         "WiFi": {
           "EAP": {
             "Identity": "identity_value",
             "ClientCertType": "Ref",
             "ClientCertRef": "guid"
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kEap);
  EXPECT_EQ(cert_config.client_cert_type, ::onc::client_cert::kRef);
  EXPECT_EQ(cert_config.guid, "guid");
  EXPECT_EQ(cert_config.provisioning_profile_id, std::string());
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

TEST(ClientCertUtilTest, OncToClientCertConfig_Wifi_ProvisioningProfileId) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "WiFi",
         "WiFi": {
           "EAP": {
             "Identity": "identity_value",
             "ClientCertType": "ProvisioningProfileId",
             "ClientCertProvisioningProfileId": "profile_id"
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kEap);
  EXPECT_EQ(cert_config.client_cert_type,
            ::onc::client_cert::kProvisioningProfileId);
  EXPECT_EQ(cert_config.guid, std::string());
  EXPECT_EQ(cert_config.provisioning_profile_id, "profile_id");
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

// The tests for the other network types (Ethernet, OpenVPN, ...) are performed
// just for provisioning profile id as an example.

TEST(ClientCertUtilTest, OncToClientCertConfig_Ethernet_ProvisioningProfileId) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "Ethernet",
         "Ethernet": {
           "EAP": {
             "Identity": "identity_value",
             "ClientCertType": "ProvisioningProfileId",
             "ClientCertProvisioningProfileId": "profile_id"
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kEap);
  EXPECT_EQ(cert_config.client_cert_type,
            ::onc::client_cert::kProvisioningProfileId);
  EXPECT_EQ(cert_config.guid, std::string());
  EXPECT_EQ(cert_config.provisioning_profile_id, "profile_id");
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

TEST(ClientCertUtilTest, OncToClientCertConfig_OpenVPN_ProvisioningProfileId) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "VPN",
         "VPN": {
           "OpenVPN": {
             "Identity": "identity_value",
             "ClientCertType": "ProvisioningProfileId",
             "ClientCertProvisioningProfileId": "profile_id"
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kOpenVpn);
  EXPECT_EQ(cert_config.client_cert_type,
            ::onc::client_cert::kProvisioningProfileId);
  EXPECT_EQ(cert_config.guid, std::string());
  EXPECT_EQ(cert_config.provisioning_profile_id, "profile_id");
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

TEST(ClientCertUtilTest,
     OncToClientCertConfig_L2TPIPsec_ProvisioningProfileId) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "VPN",
         "VPN": {
           "L2TP": { },
           "IPsec": {
             "Identity": "identity_value",
             "ClientCertType": "ProvisioningProfileId",
             "ClientCertProvisioningProfileId": "profile_id"
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kL2tpIpsec);
  EXPECT_EQ(cert_config.client_cert_type,
            ::onc::client_cert::kProvisioningProfileId);
  EXPECT_EQ(cert_config.guid, std::string());
  EXPECT_EQ(cert_config.provisioning_profile_id, "profile_id");
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

TEST(ClientCertUtilTest, OncToClientCertConfig_IKEv2_ProvisioningProfileId) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "VPN",
         "VPN": {
           "IPsec": {
             "Identity": "identity_value",
             "ClientCertType": "ProvisioningProfileId",
             "ClientCertProvisioningProfileId": "profile_id"
           }
         }
       })");
  ClientCertConfig cert_config;
  OncToClientCertConfig(::onc::ONC_SOURCE_USER_POLICY, network_config,
                        &cert_config);

  EXPECT_EQ(cert_config.location, ConfigType::kIkev2);
  EXPECT_EQ(cert_config.client_cert_type,
            ::onc::client_cert::kProvisioningProfileId);
  EXPECT_EQ(cert_config.guid, std::string());
  EXPECT_EQ(cert_config.provisioning_profile_id, "profile_id");
  EXPECT_EQ(cert_config.policy_identity, "identity_value");
  EXPECT_EQ(cert_config.onc_source, ::onc::ONC_SOURCE_USER_POLICY);
}

TEST(ClientCertUtilTest, SetResolvedCertForEthernetEap) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "Ethernet",
         "Ethernet": {
           "Authentication": "8021X",
           "EAP": {
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Subject": {
                 "CommonName": "Test"
               }
             }
           }
         }
       })");
  SetResolvedCertInOnc(ResolvedCert::CertMatched(1, "pkcs11_id", {}),
                       network_config);
  EXPECT_THAT(network_config, base::test::IsJson(
                                  R"({
         "Type": "Ethernet",
         "Ethernet": {
           "Authentication": "8021X",
           "EAP": {
             "ClientCertType": "PKCS11Id",
             "ClientCertPKCS11Id": "1:pkcs11_id"
           }
         }
       })"));
}

TEST(ClientCertUtilTest, SetResolvedCertForWifiEap) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "WiFi",
         "WiFi": {
           "Security": "WPA-EAP",
           "EAP": {
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Subject": {
                 "CommonName": "Test"
               }
             }
           }
         }
       })");
  SetResolvedCertInOnc(ResolvedCert::CertMatched(1, "pkcs11_id", {}),
                       network_config);
  EXPECT_THAT(network_config, base::test::IsJson(
                                  R"({
         "Type": "WiFi",
         "WiFi": {
           "Security": "WPA-EAP",
           "EAP": {
             "ClientCertType": "PKCS11Id",
             "ClientCertPKCS11Id": "1:pkcs11_id"
           }
         }
       })"));
}

TEST(ClientCertUtilTest, SetResolvedCertForOpenVpn) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "VPN",
         "VPN": {
           "Type": "OpenVPN",
           "OpenVPN": {
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Subject": {
                 "CommonName": "Test"
               }
             }
           }
         }
       })");
  SetResolvedCertInOnc(ResolvedCert::CertMatched(1, "pkcs11_id", {}),
                       network_config);
  EXPECT_THAT(network_config, base::test::IsJson(
                                  R"({
         "Type": "VPN",
         "VPN": {
           "Type": "OpenVPN",
           "OpenVPN": {
             "ClientCertType": "PKCS11Id",
             "ClientCertPKCS11Id": "1:pkcs11_id"
           }
         }
       })"));
}

TEST(ClientCertUtilTest, SetResolvedCertForLt2pIpsecVpn) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "VPN",
         "VPN": {
           "Type": "L2TP-IPsec",
           "IPsec": {
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Subject": {
                 "CommonName": "Test"
               }
             }
           }
         }
       })");
  SetResolvedCertInOnc(ResolvedCert::CertMatched(1, "pkcs11_id", {}),
                       network_config);
  EXPECT_THAT(network_config, base::test::IsJson(
                                  R"({
         "Type": "VPN",
         "VPN": {
           "Type": "L2TP-IPsec",
           "IPsec": {
             "ClientCertType": "PKCS11Id",
             "ClientCertPKCS11Id": "1:pkcs11_id"
           }
         }
       })"));
}

// Tests that the NotKnownYet state doesn't change the ONC value.
TEST(ClientCertUtilTest, NotKnownYet) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "Ethernet",
         "Ethernet": {
           "Authentication": "8021X",
           "EAP": {
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Subject": {
                 "CommonName": "Test"
               }
             }
           }
         }
       })");
  base::Value::Dict network_config_orig = network_config.Clone();
  SetResolvedCertInOnc(ResolvedCert::NotKnownYet(), network_config);
  EXPECT_THAT(network_config, base::test::IsJson(network_config_orig));
}

TEST(ClientCertUtilTest, NoCert) {
  base::Value::Dict network_config = base::test::ParseJsonDict(
      R"({
         "Type": "Ethernet",
         "Ethernet": {
           "Authentication": "8021X",
           "EAP": {
             "ClientCertType": "Pattern",
             "ClientCertPattern": {
               "Subject": {
                 "CommonName": "Test"
               }
             }
           }
         }
       })");
  SetResolvedCertInOnc(ResolvedCert::NothingMatched(), network_config);
  EXPECT_THAT(network_config, base::test::IsJson(
                                  R"({
         "Type": "Ethernet",
         "Ethernet": {
           "Authentication": "8021X",
           "EAP": {
             "ClientCertType": "PKCS11Id",
             "ClientCertPKCS11Id": ""
           }
         }
       })"));
}

}  // namespace ash::client_cert
