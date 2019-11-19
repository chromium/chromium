// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_signature.h"

#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using base::Value;

namespace chromeos {
namespace onc {
namespace {

const OncValueSignature kBoolSignature = {base::Value::Type::BOOLEAN, NULL};
const OncValueSignature kStringSignature = {base::Value::Type::STRING, NULL};
const OncValueSignature kIntegerSignature = {base::Value::Type::INTEGER, NULL};
const OncValueSignature kStringListSignature = {base::Value::Type::LIST, NULL,
                                                &kStringSignature};
const OncValueSignature kIntegerListSignature = {base::Value::Type::LIST, NULL,
                                                 &kIntegerSignature};
const OncValueSignature kIPConfigListSignature = {base::Value::Type::LIST, NULL,
                                                  &kIPConfigSignature};
const OncValueSignature kCellularApnListSignature = {
    base::Value::Type::LIST, NULL, &kCellularApnSignature};
const OncValueSignature kCellularFoundNetworkListSignature = {
    base::Value::Type::LIST, NULL, &kCellularFoundNetworkSignature};

const OncFieldSignature issuer_subject_pattern_fields[] = {
    {::onc::client_cert::kCommonName, &kStringSignature},
    {::onc::client_cert::kLocality, &kStringSignature},
    {::onc::client_cert::kOrganization, &kStringSignature},
    {::onc::client_cert::kOrganizationalUnit, &kStringSignature},
    {NULL}};

const OncFieldSignature certificate_pattern_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::client_cert::kEnrollmentURI, &kStringListSignature},
    {::onc::client_cert::kIssuer, &kIssuerSubjectPatternSignature},
    {::onc::client_cert::kIssuerCARef, &kStringListSignature},
    // Used internally. Not officially supported.
    {::onc::client_cert::kIssuerCAPEMs, &kStringListSignature},
    {::onc::client_cert::kSubject, &kIssuerSubjectPatternSignature},
    {NULL}};

const OncFieldSignature eap_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::eap::kAnonymousIdentity, &kStringSignature},
    {::onc::client_cert::kClientCertPKCS11Id, &kStringSignature},
    {::onc::client_cert::kClientCertPattern, &kCertificatePatternSignature},
    {::onc::client_cert::kClientCertRef, &kStringSignature},
    {::onc::client_cert::kClientCertType, &kStringSignature},
    {::onc::eap::kIdentity, &kStringSignature},
    {::onc::eap::kInner, &kStringSignature},
    {::onc::eap::kOuter, &kStringSignature},
    {::onc::eap::kPassword, &kStringSignature},
    {::onc::eap::kSaveCredentials, &kBoolSignature},
    // Used internally. Not officially supported.
    {::onc::eap::kServerCAPEMs, &kStringListSignature},
    // Deprecated.
    {::onc::eap::kServerCARef, &kStringSignature},
    {::onc::eap::kServerCARefs, &kStringListSignature},
    {::onc::eap::kSubjectMatch, &kStringSignature},
    {::onc::eap::kTLSVersionMax, &kStringSignature},
    {::onc::eap::kUseProactiveKeyCaching, &kBoolSignature},
    {::onc::eap::kUseSystemCAs, &kBoolSignature},
    {NULL}};

const OncFieldSignature ipsec_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::ipsec::kAuthenticationType, &kStringSignature},
    {::onc::client_cert::kClientCertPKCS11Id, &kStringSignature},
    {::onc::client_cert::kClientCertPattern, &kCertificatePatternSignature},
    {::onc::client_cert::kClientCertRef, &kStringSignature},
    {::onc::client_cert::kClientCertType, &kStringSignature},
    {::onc::ipsec::kGroup, &kStringSignature},
    {::onc::ipsec::kIKEVersion, &kIntegerSignature},
    {::onc::ipsec::kPSK, &kStringSignature},
    {::onc::vpn::kSaveCredentials, &kBoolSignature},
    // Used internally. Not officially supported.
    {::onc::ipsec::kServerCAPEMs, &kStringListSignature},
    {::onc::ipsec::kServerCARef, &kStringSignature},
    {::onc::ipsec::kServerCARefs, &kStringListSignature},
    {::onc::ipsec::kXAUTH, &kXAUTHSignature},
    // Not yet supported.
    //  { ipsec::kEAP, &kEAPSignature },
    {NULL}};

const OncFieldSignature xauth_fields[] = {
    {::onc::vpn::kPassword, &kStringSignature},
    {::onc::vpn::kUsername, &kStringSignature},
    {NULL}};

const OncFieldSignature l2tp_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::l2tp::kPassword, &kStringSignature},
    {::onc::l2tp::kSaveCredentials, &kBoolSignature},
    {::onc::l2tp::kUsername, &kStringSignature},
    {::onc::l2tp::kLcpEchoDisabled, &kBoolSignature},
    {NULL}};

const OncFieldSignature openvpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::openvpn::kAuth, &kStringSignature},
    {::onc::openvpn::kAuthNoCache, &kBoolSignature},
    {::onc::openvpn::kAuthRetry, &kStringSignature},
    {::onc::openvpn::kCipher, &kStringSignature},
    {::onc::client_cert::kClientCertPKCS11Id, &kStringSignature},
    {::onc::client_cert::kClientCertPattern, &kCertificatePatternSignature},
    {::onc::client_cert::kClientCertRef, &kStringSignature},
    {::onc::client_cert::kClientCertType, &kStringSignature},
    {::onc::openvpn::kCompLZO, &kStringSignature},
    {::onc::openvpn::kCompNoAdapt, &kBoolSignature},
    {::onc::openvpn::kExtraHosts, &kStringListSignature},
    {::onc::openvpn::kIgnoreDefaultRoute, &kBoolSignature},
    {::onc::openvpn::kKeyDirection, &kStringSignature},
    {::onc::openvpn::kNsCertType, &kStringSignature},
    {::onc::openvpn::kOTP, &kStringSignature},
    {::onc::openvpn::kPassword, &kStringSignature},
    {::onc::openvpn::kPort, &kIntegerSignature},
    {::onc::openvpn::kProto, &kStringSignature},
    {::onc::openvpn::kPushPeerInfo, &kBoolSignature},
    {::onc::openvpn::kRemoteCertEKU, &kStringSignature},
    {::onc::openvpn::kRemoteCertKU, &kStringListSignature},
    {::onc::openvpn::kRemoteCertTLS, &kStringSignature},
    {::onc::openvpn::kRenegSec, &kIntegerSignature},
    {::onc::vpn::kSaveCredentials, &kBoolSignature},
    // Used internally. Not officially supported.
    {::onc::openvpn::kServerCAPEMs, &kStringListSignature},
    {::onc::openvpn::kServerCARef, &kStringSignature},
    {::onc::openvpn::kServerCARefs, &kStringListSignature},
    // Not supported, yet.
    {::onc::openvpn::kServerCertPEM, &kStringSignature},
    {::onc::openvpn::kServerCertRef, &kStringSignature},
    {::onc::openvpn::kServerPollTimeout, &kIntegerSignature},
    {::onc::openvpn::kShaper, &kIntegerSignature},
    {::onc::openvpn::kStaticChallenge, &kStringSignature},
    {::onc::openvpn::kTLSAuthContents, &kStringSignature},
    {::onc::openvpn::kTLSRemote, &kStringSignature},
    {::onc::openvpn::kTLSVersionMin, &kStringSignature},
    {::onc::openvpn::kUserAuthenticationType, &kStringSignature},
    {::onc::vpn::kUsername, &kStringSignature},
    {::onc::openvpn::kVerb, &kStringSignature},
    {::onc::openvpn::kVerifyHash, &kStringSignature},
    {::onc::openvpn::kVerifyX509, &kVerifyX509Signature},
    {NULL}};

const OncFieldSignature third_party_vpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::third_party_vpn::kExtensionID, &kStringSignature},
    {NULL}};

const OncFieldSignature arc_vpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::arc_vpn::kTunnelChrome, &kStringSignature},
    {NULL}};

const OncFieldSignature verify_x509_fields[] = {
    {::onc::verify_x509::kName, &kStringSignature},
    {::onc::verify_x509::kType, &kStringSignature},
    {NULL}};

const OncFieldSignature vpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::vpn::kAutoConnect, &kBoolSignature},
    {::onc::vpn::kHost, &kStringSignature},
    {::onc::vpn::kIPsec, &kIPsecSignature},
    {::onc::vpn::kL2TP, &kL2TPSignature},
    {::onc::vpn::kOpenVPN, &kOpenVPNSignature},
    {::onc::vpn::kThirdPartyVpn, &kThirdPartyVPNSignature},
    {::onc::vpn::kArcVpn, &kARCVPNSignature},
    {::onc::vpn::kType, &kStringSignature},
    {NULL}};

const OncFieldSignature ethernet_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::ethernet::kAuthentication, &kStringSignature},
    {::onc::ethernet::kEAP, &kEAPSignature},
    {NULL}};

const OncFieldSignature tether_fields[] = {{NULL}};

const OncFieldSignature tether_with_state_fields[] = {
    {::onc::tether::kBatteryPercentage, &kIntegerSignature},
    {::onc::tether::kCarrier, &kStringSignature},
    {::onc::tether::kHasConnectedToHost, &kBoolSignature},
    {::onc::tether::kSignalStrength, &kIntegerSignature},
    {NULL}};

const OncFieldSignature ipconfig_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::ipconfig::kGateway, &kStringSignature},
    {::onc::ipconfig::kIPAddress, &kStringSignature},
    {::onc::ipconfig::kNameServers, &kStringListSignature},
    {::onc::ipconfig::kRoutingPrefix, &kIntegerSignature},
    {::onc::ipconfig::kSearchDomains, &kStringListSignature},
    {::onc::ipconfig::kIncludedRoutes, &kStringListSignature},
    {::onc::ipconfig::kExcludedRoutes, &kStringListSignature},
    {::onc::ipconfig::kType, &kStringSignature},
    {::onc::ipconfig::kWebProxyAutoDiscoveryUrl, &kStringSignature},
    {NULL}};

const OncFieldSignature proxy_location_fields[] = {
    {::onc::proxy::kHost, &kStringSignature},
    {::onc::proxy::kPort, &kIntegerSignature},
    {NULL}};

const OncFieldSignature proxy_manual_fields[] = {
    {::onc::proxy::kFtp, &kProxyLocationSignature},
    {::onc::proxy::kHttp, &kProxyLocationSignature},
    {::onc::proxy::kHttps, &kProxyLocationSignature},
    {::onc::proxy::kSocks, &kProxyLocationSignature},
    {NULL}};

const OncFieldSignature proxy_settings_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::proxy::kExcludeDomains, &kStringListSignature},
    {::onc::proxy::kManual, &kProxyManualSignature},
    {::onc::proxy::kPAC, &kStringSignature},
    {::onc::proxy::kType, &kStringSignature},
    {NULL}};

const OncFieldSignature wifi_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::wifi::kAllowGatewayARPPolling, &kBoolSignature},
    {::onc::wifi::kAutoConnect, &kBoolSignature},
    {::onc::wifi::kEAP, &kEAPSignature},
    {::onc::wifi::kFTEnabled, &kBoolSignature},
    {::onc::wifi::kHexSSID, &kStringSignature},
    {::onc::wifi::kHiddenSSID, &kBoolSignature},
    {::onc::wifi::kPassphrase, &kStringSignature},
    {::onc::wifi::kRoamThreshold, &kIntegerSignature},
    {::onc::wifi::kSSID, &kStringSignature},
    {::onc::wifi::kSecurity, &kStringSignature},
    {NULL}};

const OncFieldSignature wifi_with_state_fields[] = {
    {::onc::wifi::kBSSID, &kStringSignature},
    {::onc::wifi::kFrequency, &kIntegerSignature},
    {::onc::wifi::kFrequencyList, &kIntegerListSignature},
    {::onc::wifi::kSignalStrength, &kIntegerSignature},
    {::onc::wifi::kTetheringState, &kStringSignature},
    {NULL}};

const OncFieldSignature cellular_payment_portal_fields[] = {
    {::onc::cellular_payment_portal::kMethod, &kStringSignature},
    {::onc::cellular_payment_portal::kPostData, &kStringSignature},
    {::onc::cellular_payment_portal::kUrl, &kStringSignature},
    {NULL}};

const OncFieldSignature cellular_provider_fields[] = {
    {::onc::cellular_provider::kCode, &kStringSignature},
    {::onc::cellular_provider::kCountry, &kStringSignature},
    {::onc::cellular_provider::kName, &kStringSignature},
    {NULL}};

const OncFieldSignature cellular_apn_fields[] = {
    {::onc::cellular_apn::kAccessPointName, &kStringSignature},
    {::onc::cellular_apn::kName, &kStringSignature},
    {::onc::cellular_apn::kUsername, &kStringSignature},
    {::onc::cellular_apn::kPassword, &kStringSignature},
    {::onc::cellular_apn::kAuthentication, &kStringSignature},
    {::onc::cellular_apn::kLocalizedName, &kStringSignature},
    {::onc::cellular_apn::kLanguage, &kStringSignature},
    {NULL}};

const OncFieldSignature cellular_found_network_fields[] = {
    {::onc::cellular_found_network::kStatus, &kStringSignature},
    {::onc::cellular_found_network::kNetworkId, &kStringSignature},
    {::onc::cellular_found_network::kShortName, &kStringSignature},
    {::onc::cellular_found_network::kLongName, &kStringSignature},
    {::onc::cellular_found_network::kTechnology, &kStringSignature},
    {NULL}};

const OncFieldSignature sim_lock_status_fields[] = {
    {::onc::sim_lock_status::kLockEnabled, &kBoolSignature},
    {::onc::sim_lock_status::kLockType, &kStringSignature},
    {::onc::sim_lock_status::kRetriesLeft, &kIntegerSignature},
    {NULL}};

const OncFieldSignature cellular_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::cellular::kAPN, &kCellularApnSignature},
    {::onc::cellular::kAPNList, &kCellularApnListSignature},
    {::onc::cellular::kCarrier, &kStringSignature},
    {::onc::cellular::kAutoConnect, &kBoolSignature},
    {NULL}};

const OncFieldSignature cellular_with_state_fields[] = {
    {::onc::cellular::kActivationType, &kStringSignature},
    {::onc::cellular::kActivationState, &kStringSignature},
    {::onc::cellular::kAllowRoaming, &kBoolSignature},
    {::onc::cellular::kESN, &kStringSignature},
    {::onc::cellular::kFamily, &kStringSignature},
    {::onc::cellular::kFirmwareRevision, &kStringSignature},
    {::onc::cellular::kFoundNetworks, &kCellularFoundNetworkListSignature},
    {::onc::cellular::kHardwareRevision, &kStringSignature},
    {::onc::cellular::kHomeProvider, &kCellularProviderSignature},
    {::onc::cellular::kICCID, &kStringSignature},
    {::onc::cellular::kIMEI, &kStringSignature},
    {::onc::cellular::kIMSI, &kStringSignature},
    {::onc::cellular::kLastGoodAPN, &kCellularApnSignature},
    {::onc::cellular::kManufacturer, &kStringSignature},
    {::onc::cellular::kMDN, &kStringSignature},
    {::onc::cellular::kMEID, &kStringSignature},
    {::onc::cellular::kMIN, &kStringSignature},
    {::onc::cellular::kModelID, &kStringSignature},
    {::onc::cellular::kNetworkTechnology, &kStringSignature},
    {::onc::cellular::kPaymentPortal, &kCellularPaymentPortalSignature},
    {::onc::cellular::kRoamingState, &kStringSignature},
    {::onc::cellular::kScanning, &kBoolSignature},
    {::onc::cellular::kServingOperator, &kCellularProviderSignature},
    {::onc::cellular::kSignalStrength, &kIntegerSignature},
    {::onc::cellular::kSIMLockStatus, &kSIMLockStatusSignature},
    {::onc::cellular::kSIMPresent, &kBoolSignature},
    {::onc::cellular::kSupportNetworkScan, &kBoolSignature},
    {NULL}};

const OncFieldSignature network_configuration_fields[] = {
    {::onc::network_config::kCellular, &kCellularSignature},
    {::onc::network_config::kEthernet, &kEthernetSignature},
    {::onc::network_config::kGUID, &kStringSignature},
    {::onc::network_config::kIPAddressConfigType, &kStringSignature},
    {::onc::network_config::kName, &kStringSignature},
    {::onc::network_config::kNameServersConfigType, &kStringSignature},
    {::onc::network_config::kPriority, &kIntegerSignature},
    {::onc::network_config::kProxySettings, &kProxySettingsSignature},
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::kRemove, &kBoolSignature},
    {::onc::network_config::kStaticIPConfig, &kStaticIPConfigSignature},
    {::onc::network_config::kTether, &kTetherSignature},
    {::onc::network_config::kType, &kStringSignature},
    {::onc::network_config::kVPN, &kVPNSignature},
    {::onc::network_config::kWiFi, &kWiFiSignature},
    {NULL}};

const OncFieldSignature network_with_state_fields[] = {
    {::onc::network_config::kCellular, &kCellularWithStateSignature},
    {::onc::network_config::kConnectionState, &kStringSignature},
    {::onc::network_config::kConnectable, &kBoolSignature},
    {::onc::network_config::kErrorState, &kStringSignature},
    {::onc::network_config::kIPConfigs, &kIPConfigListSignature},
    {::onc::network_config::kMacAddress, &kStringSignature},
    {::onc::network_config::kRestrictedConnectivity, &kBoolSignature},
    {::onc::network_config::kSavedIPConfig, &kSavedIPConfigSignature},
    {::onc::network_config::kSource, &kStringSignature},
    {::onc::network_config::kTether, &kTetherWithStateSignature},
    {::onc::network_config::kWiFi, &kWiFiWithStateSignature},
    {NULL}};

const OncFieldSignature global_network_configuration_fields[] = {
    {::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
     &kBoolSignature},
    {::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
     &kBoolSignature},
    {::onc::global_network_config::kAllowOnlyPolicyNetworksToConnectIfAvailable,
     &kBoolSignature},
    {::onc::global_network_config::kBlacklistedHexSSIDs, &kStringListSignature},
    {::onc::global_network_config::kDisableNetworkTypes, &kStringListSignature},
    {NULL}};

const OncFieldSignature certificate_fields[] = {
    {::onc::certificate::kGUID, &kStringSignature},
    {::onc::certificate::kScope, &kScopeSignature},
    {::onc::certificate::kPKCS12, &kStringSignature},
    {::onc::kRemove, &kBoolSignature},
    {::onc::certificate::kTrustBits, &kStringListSignature},
    {::onc::certificate::kType, &kStringSignature},
    {::onc::certificate::kX509, &kStringSignature},
    {NULL}};

const OncFieldSignature scope_fields[] = {
    {::onc::scope::kType, &kStringSignature},
    {::onc::scope::kId, &kStringSignature},
    {nullptr}};

const OncFieldSignature toplevel_configuration_fields[] = {
    {::onc::toplevel_config::kCertificates, &kCertificateListSignature},
    {::onc::toplevel_config::kNetworkConfigurations,
     &kNetworkConfigurationListSignature},
    {::onc::toplevel_config::kGlobalNetworkConfiguration,
     &kGlobalNetworkConfigurationSignature},
    {::onc::toplevel_config::kType, &kStringSignature},
    {::onc::encrypted::kCipher, &kStringSignature},
    {::onc::encrypted::kCiphertext, &kStringSignature},
    {::onc::encrypted::kHMAC, &kStringSignature},
    {::onc::encrypted::kHMACMethod, &kStringSignature},
    {::onc::encrypted::kIV, &kStringSignature},
    {::onc::encrypted::kIterations, &kIntegerSignature},
    {::onc::encrypted::kSalt, &kStringSignature},
    {::onc::encrypted::kStretch, &kStringSignature},
    {NULL}};

}  // namespace

const OncValueSignature kRecommendedSignature = {base::Value::Type::LIST, NULL,
                                                 &kStringSignature};
const OncValueSignature kEAPSignature = {base::Value::Type::DICTIONARY,
                                         eap_fields, NULL};
const OncValueSignature kIssuerSubjectPatternSignature = {
    base::Value::Type::DICTIONARY, issuer_subject_pattern_fields, NULL};
const OncValueSignature kCertificatePatternSignature = {
    base::Value::Type::DICTIONARY, certificate_pattern_fields, NULL};
const OncValueSignature kIPsecSignature = {base::Value::Type::DICTIONARY,
                                           ipsec_fields, NULL};
const OncValueSignature kXAUTHSignature = {base::Value::Type::DICTIONARY,
                                           xauth_fields, NULL};
const OncValueSignature kL2TPSignature = {base::Value::Type::DICTIONARY,
                                          l2tp_fields, NULL};
const OncValueSignature kOpenVPNSignature = {base::Value::Type::DICTIONARY,
                                             openvpn_fields, NULL};
const OncValueSignature kThirdPartyVPNSignature = {
    base::Value::Type::DICTIONARY, third_party_vpn_fields, NULL};
const OncValueSignature kARCVPNSignature = {base::Value::Type::DICTIONARY,
                                            arc_vpn_fields, NULL};
const OncValueSignature kVerifyX509Signature = {base::Value::Type::DICTIONARY,
                                                verify_x509_fields, NULL};
const OncValueSignature kVPNSignature = {base::Value::Type::DICTIONARY,
                                         vpn_fields, NULL};
const OncValueSignature kEthernetSignature = {base::Value::Type::DICTIONARY,
                                              ethernet_fields, NULL};
const OncValueSignature kIPConfigSignature = {base::Value::Type::DICTIONARY,
                                              ipconfig_fields, NULL};
const OncValueSignature kSavedIPConfigSignature = {
    base::Value::Type::DICTIONARY, ipconfig_fields, NULL};
const OncValueSignature kStaticIPConfigSignature = {
    base::Value::Type::DICTIONARY, ipconfig_fields, NULL};
const OncValueSignature kProxyLocationSignature = {
    base::Value::Type::DICTIONARY, proxy_location_fields, NULL};
const OncValueSignature kProxyManualSignature = {base::Value::Type::DICTIONARY,
                                                 proxy_manual_fields, NULL};
const OncValueSignature kProxySettingsSignature = {
    base::Value::Type::DICTIONARY, proxy_settings_fields, NULL};
const OncValueSignature kWiFiSignature = {base::Value::Type::DICTIONARY,
                                          wifi_fields, NULL};
const OncValueSignature kCertificateSignature = {base::Value::Type::DICTIONARY,
                                                 certificate_fields, NULL};
const OncValueSignature kScopeSignature = {base::Value::Type::DICTIONARY,
                                           scope_fields, nullptr};
const OncValueSignature kNetworkConfigurationSignature = {
    base::Value::Type::DICTIONARY, network_configuration_fields, NULL};
const OncValueSignature kGlobalNetworkConfigurationSignature = {
    base::Value::Type::DICTIONARY, global_network_configuration_fields, NULL};
const OncValueSignature kCertificateListSignature = {
    base::Value::Type::LIST, NULL, &kCertificateSignature};
const OncValueSignature kNetworkConfigurationListSignature = {
    base::Value::Type::LIST, NULL, &kNetworkConfigurationSignature};
const OncValueSignature kToplevelConfigurationSignature = {
    base::Value::Type::DICTIONARY, toplevel_configuration_fields, NULL};

// Derived "ONC with State" signatures.
const OncValueSignature kNetworkWithStateSignature = {
    base::Value::Type::DICTIONARY, network_with_state_fields, NULL,
    &kNetworkConfigurationSignature};
const OncValueSignature kWiFiWithStateSignature = {
    base::Value::Type::DICTIONARY, wifi_with_state_fields, NULL,
    &kWiFiSignature};
const OncValueSignature kTetherSignature = {base::Value::Type::DICTIONARY,
                                            tether_fields, NULL};
const OncValueSignature kTetherWithStateSignature = {
    base::Value::Type::DICTIONARY, tether_with_state_fields, NULL,
    &kTetherSignature};
const OncValueSignature kCellularSignature = {base::Value::Type::DICTIONARY,
                                              cellular_fields, NULL};
const OncValueSignature kCellularWithStateSignature = {
    base::Value::Type::DICTIONARY, cellular_with_state_fields, NULL,
    &kCellularSignature};
const OncValueSignature kCellularPaymentPortalSignature = {
    base::Value::Type::DICTIONARY, cellular_payment_portal_fields, NULL};
const OncValueSignature kCellularProviderSignature = {
    base::Value::Type::DICTIONARY, cellular_provider_fields, NULL};
const OncValueSignature kCellularApnSignature = {base::Value::Type::DICTIONARY,
                                                 cellular_apn_fields, NULL};
const OncValueSignature kCellularFoundNetworkSignature = {
    base::Value::Type::DICTIONARY, cellular_found_network_fields, NULL};
const OncValueSignature kSIMLockStatusSignature = {
    base::Value::Type::DICTIONARY, sim_lock_status_fields, NULL};

const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name) {
  if (!signature.fields)
    return NULL;
  for (const OncFieldSignature* field_signature = signature.fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    if (onc_field_name == field_signature->onc_field_name)
      return field_signature;
  }
  if (signature.base_signature)
    return GetFieldSignature(*signature.base_signature, onc_field_name);
  return NULL;
}

namespace {

struct CredentialEntry {
  const OncValueSignature* value_signature;
  const char* field_name;
};

const CredentialEntry credentials[] = {
    {&kEAPSignature, ::onc::eap::kPassword},
    {&kIPsecSignature, ::onc::ipsec::kPSK},
    {&kXAUTHSignature, ::onc::vpn::kPassword},
    {&kL2TPSignature, ::onc::vpn::kPassword},
    {&kOpenVPNSignature, ::onc::vpn::kPassword},
    {&kOpenVPNSignature, ::onc::openvpn::kTLSAuthContents},
    {&kWiFiSignature, ::onc::wifi::kPassphrase},
    {&kCellularApnSignature, ::onc::cellular_apn::kPassword},
    // While not really a credential, PKCS12 blobs may contain unencrypted
    // private keys.
    {&kCertificateSignature, ::onc::certificate::kPKCS12},
    {NULL}};

}  // namespace

bool FieldIsCredential(const OncValueSignature& signature,
                       const std::string& onc_field_name) {
  for (const CredentialEntry* entry = credentials;
       entry->value_signature != NULL; ++entry) {
    if (&signature == entry->value_signature &&
        onc_field_name == entry->field_name) {
      return true;
    }
  }
  return false;
}

}  // namespace onc
}  // namespace chromeos
