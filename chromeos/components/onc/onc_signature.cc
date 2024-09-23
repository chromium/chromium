// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/components/onc/onc_signature.h"

#include "base/memory/raw_ptr_exclusion.h"
#include "components/onc/onc_constants.h"
using base::Value;

namespace chromeos {
namespace onc {
namespace {

const OncValueSignature kBoolSignature = {base::Value::Type::BOOLEAN, nullptr};
const OncValueSignature kStringSignature = {base::Value::Type::STRING, nullptr};
const OncValueSignature kIntegerSignature = {base::Value::Type::INTEGER,
                                             nullptr};
const OncValueSignature kDoubleSignature = {base::Value::Type::DOUBLE, nullptr};
const OncValueSignature kStringListSignature = {base::Value::Type::LIST,
                                                nullptr, &kStringSignature};
const OncValueSignature kIntegerListSignature = {base::Value::Type::LIST,
                                                 nullptr, &kIntegerSignature};
const OncValueSignature kIPConfigListSignature = {base::Value::Type::LIST,
                                                  nullptr, &kIPConfigSignature};
const OncValueSignature kCellularApnListSignature = {
    base::Value::Type::LIST, nullptr, &kCellularApnSignature};
const OncValueSignature kCellularFoundNetworkListSignature = {
    base::Value::Type::LIST, nullptr, &kCellularFoundNetworkSignature};
const OncValueSignature kEAPSubjectAlternativeNameMatchListSignature = {
    base::Value::Type::LIST, nullptr,
    &kEAPSubjectAlternativeNameMatchSignature};

const OncFieldSignature issuer_subject_pattern_fields[] = {
    {::onc::client_cert::kCommonName, &kStringSignature},
    {::onc::client_cert::kLocality, &kStringSignature},
    {::onc::client_cert::kOrganization, &kStringSignature},
    {::onc::client_cert::kOrganizationalUnit, &kStringSignature},
    {nullptr}};

const OncFieldSignature certificate_pattern_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::client_cert::kEnrollmentURI, &kStringListSignature},
    {::onc::client_cert::kIssuer, &kIssuerSubjectPatternSignature},
    {::onc::client_cert::kIssuerCARef, &kStringListSignature},
    // Used internally. Not officially supported.
    {::onc::client_cert::kIssuerCAPEMs, &kStringListSignature},
    {::onc::client_cert::kSubject, &kIssuerSubjectPatternSignature},
    {nullptr}};

const OncFieldSignature eap_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::eap::kAnonymousIdentity, &kStringSignature},
    {::onc::client_cert::kClientCertPKCS11Id, &kStringSignature},
    {::onc::client_cert::kClientCertPattern, &kCertificatePatternSignature},
    {::onc::client_cert::kClientCertProvisioningProfileId, &kStringSignature},
    {::onc::client_cert::kClientCertRef, &kStringSignature},
    {::onc::client_cert::kClientCertType, &kStringSignature},
    {::onc::eap::kDomainSuffixMatch, &kStringListSignature},
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
    {::onc::eap::kSubjectAlternativeNameMatch,
     &kEAPSubjectAlternativeNameMatchListSignature},
    {::onc::eap::kTLSVersionMax, &kStringSignature},
    {::onc::eap::kUseProactiveKeyCaching, &kBoolSignature},
    {::onc::eap::kUseSystemCAs, &kBoolSignature},
    {nullptr}};

const OncFieldSignature ipsec_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::ipsec::kAuthenticationType, &kStringSignature},
    {::onc::client_cert::kClientCertPKCS11Id, &kStringSignature},
    {::onc::client_cert::kClientCertPattern, &kCertificatePatternSignature},
    {::onc::client_cert::kClientCertProvisioningProfileId, &kStringSignature},
    {::onc::client_cert::kClientCertRef, &kStringSignature},
    {::onc::client_cert::kClientCertType, &kStringSignature},
    {::onc::ipsec::kGroup, &kStringSignature},
    {::onc::ipsec::kIKEVersion, &kIntegerSignature},
    {::onc::ipsec::kLocalIdentity, &kStringSignature},
    {::onc::ipsec::kPSK, &kStringSignature},
    {::onc::ipsec::kRemoteIdentity, &kStringSignature},
    {::onc::vpn::kSaveCredentials, &kBoolSignature},
    // Used internally. Not officially supported.
    {::onc::ipsec::kServerCAPEMs, &kStringListSignature},
    {::onc::ipsec::kServerCARef, &kStringSignature},
    {::onc::ipsec::kServerCARefs, &kStringListSignature},
    {::onc::ipsec::kXAUTH, &kXAUTHSignature},
    {::onc::ipsec::kEAP, &kEAPSignature},
    {nullptr}};

const OncFieldSignature xauth_fields[] = {
    {::onc::vpn::kPassword, &kStringSignature},
    {::onc::vpn::kUsername, &kStringSignature},
    {nullptr}};

const OncFieldSignature l2tp_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::l2tp::kPassword, &kStringSignature},
    {::onc::l2tp::kSaveCredentials, &kBoolSignature},
    {::onc::l2tp::kUsername, &kStringSignature},
    {::onc::l2tp::kLcpEchoDisabled, &kBoolSignature},
    {nullptr}};

const OncFieldSignature openvpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::openvpn::kAuth, &kStringSignature},
    {::onc::openvpn::kAuthNoCache, &kBoolSignature},
    {::onc::openvpn::kAuthRetry, &kStringSignature},
    {::onc::openvpn::kCipher, &kStringSignature},
    {::onc::client_cert::kClientCertPKCS11Id, &kStringSignature},
    {::onc::client_cert::kClientCertPattern, &kCertificatePatternSignature},
    {::onc::client_cert::kClientCertProvisioningProfileId, &kStringSignature},
    {::onc::client_cert::kClientCertRef, &kStringSignature},
    {::onc::client_cert::kClientCertType, &kStringSignature},
    {::onc::openvpn::kCompLZO, &kStringSignature},
    {::onc::openvpn::kCompNoAdapt, &kBoolSignature},
    {::onc::openvpn::kCompressionAlgorithm, &kStringSignature},
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
    {nullptr}};

const OncFieldSignature wireguard_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::wireguard::kIPAddresses, &kStringListSignature},
    {::onc::wireguard::kPrivateKey, &kStringSignature},
    {::onc::wireguard::kPublicKey, &kStringSignature},
    {::onc::wireguard::kPeers, &kWireGuardPeerListSignature},
    {nullptr}};

const OncFieldSignature wireguard_peer_fields[] = {
    {::onc::wireguard::kPublicKey, &kStringSignature},
    {::onc::wireguard::kPresharedKey, &kStringSignature},
    {::onc::wireguard::kAllowedIPs, &kStringSignature},
    {::onc::wireguard::kEndpoint, &kStringSignature},
    {::onc::wireguard::kPersistentKeepalive, &kStringSignature},
    {nullptr}};

const OncFieldSignature third_party_vpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::third_party_vpn::kExtensionID, &kStringSignature},
    {nullptr}};

const OncFieldSignature arc_vpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    // Deprecated. Keeping the signature for ONC backward compatibility. See
    // b/185202698 for details.
    {::onc::arc_vpn::kTunnelChrome, &kStringSignature},
    {nullptr}};

const OncFieldSignature verify_x509_fields[] = {
    {::onc::verify_x509::kName, &kStringSignature},
    {::onc::verify_x509::kType, &kStringSignature},
    {nullptr}};

const OncFieldSignature vpn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::vpn::kAutoConnect, &kBoolSignature},
    {::onc::vpn::kHost, &kStringSignature},
    {::onc::vpn::kIPsec, &kIPsecSignature},
    {::onc::vpn::kL2TP, &kL2TPSignature},
    {::onc::vpn::kOpenVPN, &kOpenVPNSignature},
    {::onc::vpn::kWireGuard, &kWireGuardSignature},
    {::onc::vpn::kThirdPartyVpn, &kThirdPartyVPNSignature},
    {::onc::vpn::kArcVpn, &kARCVPNSignature},
    {::onc::vpn::kType, &kStringSignature},
    {nullptr}};

const OncFieldSignature ethernet_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::ethernet::kAuthentication, &kStringSignature},
    {::onc::ethernet::kEAP, &kEAPSignature},
    {nullptr}};

const OncFieldSignature tether_fields[] = {{nullptr}};

const OncFieldSignature tether_with_state_fields[] = {
    {::onc::tether::kBatteryPercentage, &kIntegerSignature},
    {::onc::tether::kCarrier, &kStringSignature},
    {::onc::tether::kHasConnectedToHost, &kBoolSignature},
    {::onc::tether::kSignalStrength, &kIntegerSignature},
    {nullptr}};

const OncFieldSignature ipconfig_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::ipconfig::kGateway, &kStringSignature},
    {::onc::ipconfig::kIPAddress, &kStringSignature},
    {::onc::ipconfig::kNameServers, &kStringListSignature},
    {::onc::ipconfig::kRoutingPrefix, &kIntegerSignature},
    {::onc::ipconfig::kSearchDomains, &kStringListSignature},
    {::onc::ipconfig::kIncludedRoutes, &kStringListSignature},
    {::onc::ipconfig::kExcludedRoutes, &kStringListSignature},
    {::onc::ipconfig::kType, &kStringSignature,
     []() { return base::Value(::onc::ipconfig::kIPv4); }},
    {::onc::ipconfig::kWebProxyAutoDiscoveryUrl, &kStringSignature},
    {::onc::ipconfig::kMTU, &kIntegerSignature},
    {nullptr}};

const OncFieldSignature proxy_location_fields[] = {
    {::onc::proxy::kHost, &kStringSignature},
    {::onc::proxy::kPort, &kIntegerSignature},
    {nullptr}};

const OncFieldSignature proxy_manual_fields[] = {
    {::onc::proxy::kFtp, &kProxyLocationSignature},
    {::onc::proxy::kHttp, &kProxyLocationSignature},
    {::onc::proxy::kHttps, &kProxyLocationSignature},
    {::onc::proxy::kSocks, &kProxyLocationSignature},
    {nullptr}};

const OncFieldSignature proxy_settings_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::proxy::kExcludeDomains, &kStringListSignature},
    {::onc::proxy::kManual, &kProxyManualSignature},
    {::onc::proxy::kPAC, &kStringSignature},
    {::onc::proxy::kType, &kStringSignature},
    {nullptr}};

const OncFieldSignature wifi_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::wifi::kAllowGatewayARPPolling, &kBoolSignature},
    {::onc::wifi::kAutoConnect, &kBoolSignature},
    {::onc::wifi::kBSSIDAllowlist, &kStringListSignature},
    {::onc::wifi::kBSSIDRequested, &kStringSignature},
    {::onc::wifi::kEAP, &kEAPSignature},
    {::onc::wifi::kHexSSID, &kStringSignature},
    {::onc::wifi::kHiddenSSID, &kBoolSignature},
    {::onc::wifi::kPassphrase, &kStringSignature},
    {::onc::wifi::kSSID, &kStringSignature},
    {::onc::wifi::kSecurity, &kStringSignature},
    {nullptr}};

const OncFieldSignature wifi_with_state_fields[] = {
    {::onc::wifi::kBSSID, &kStringSignature},
    {::onc::wifi::kFrequency, &kIntegerSignature},
    {::onc::wifi::kFrequencyList, &kIntegerListSignature},
    {::onc::wifi::kSignalStrength, &kIntegerSignature},
    {::onc::wifi::kSignalStrengthRssi, &kIntegerSignature},
    {::onc::wifi::kPasspointId, &kStringSignature},
    {::onc::wifi::kPasspointMatchType, &kStringSignature},
    {nullptr}};

const OncFieldSignature cellular_payment_portal_fields[] = {
    {::onc::cellular_payment_portal::kMethod, &kStringSignature},
    {::onc::cellular_payment_portal::kPostData, &kStringSignature},
    {::onc::cellular_payment_portal::kUrl, &kStringSignature},
    {nullptr}};

const OncFieldSignature cellular_provider_fields[] = {
    {::onc::cellular_provider::kCode, &kStringSignature},
    {::onc::cellular_provider::kCountry, &kStringSignature},
    {::onc::cellular_provider::kName, &kStringSignature},
    {nullptr}};

const OncFieldSignature cellular_apn_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::cellular_apn::kAccessPointName, &kStringSignature},
    {::onc::cellular_apn::kName, &kStringSignature},
    {::onc::cellular_apn::kUsername, &kStringSignature},
    {::onc::cellular_apn::kPassword, &kStringSignature},
    {::onc::cellular_apn::kAuthentication, &kStringSignature},
    {::onc::cellular_apn::kLocalizedName, &kStringSignature},
    {::onc::cellular_apn::kLanguage, &kStringSignature},
    {::onc::cellular_apn::kAttach, &kStringSignature},
    {::onc::cellular_apn::kId, &kStringSignature},
    {::onc::cellular_apn::kState, &kStringSignature},
    {::onc::cellular_apn::kIpType, &kStringSignature},
    {::onc::cellular_apn::kApnTypes, &kStringListSignature},
    {nullptr}};

const OncFieldSignature cellular_found_network_fields[] = {
    {::onc::cellular_found_network::kStatus, &kStringSignature},
    {::onc::cellular_found_network::kNetworkId, &kStringSignature},
    {::onc::cellular_found_network::kShortName, &kStringSignature},
    {::onc::cellular_found_network::kLongName, &kStringSignature},
    {::onc::cellular_found_network::kTechnology, &kStringSignature},
    {nullptr}};

const OncFieldSignature sim_lock_status_fields[] = {
    {::onc::sim_lock_status::kLockEnabled, &kBoolSignature},
    {::onc::sim_lock_status::kLockType, &kStringSignature},
    {::onc::sim_lock_status::kRetriesLeft, &kIntegerSignature},
    {nullptr}};

const OncFieldSignature cellular_fields[] = {
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::cellular::kAllowRoaming, &kBoolSignature},
    {::onc::cellular::kAPN, &kCellularApnSignature},
    {::onc::cellular::kAPNList, &kCellularApnListSignature},
    {::onc::cellular::kAdminAssignedAPNIds, &kStringListSignature},
    {::onc::cellular::kAutoConnect, &kBoolSignature},
    {::onc::cellular::kCustomAPNList, &kCellularApnListSignature},
    {::onc::cellular::kICCID, &kStringSignature},
    {::onc::cellular::kSMDPAddress, &kStringSignature},
    {::onc::cellular::kSMDSAddress, &kStringSignature},
    {nullptr}};

const OncFieldSignature cellular_with_state_fields[] = {
    {::onc::cellular::kActivationType, &kStringSignature},
    {::onc::cellular::kActivationState, &kStringSignature},
    {::onc::cellular::kESN, &kStringSignature},
    {::onc::cellular::kFamily, &kStringSignature},
    {::onc::cellular::kFirmwareRevision, &kStringSignature},
    {::onc::cellular::kFoundNetworks, &kCellularFoundNetworkListSignature},
    {::onc::cellular::kHardwareRevision, &kStringSignature},
    {::onc::cellular::kHomeProvider, &kCellularProviderSignature},
    {::onc::cellular::kEID, &kStringSignature},
    {::onc::cellular::kIMEI, &kStringSignature},
    {::onc::cellular::kIMSI, &kStringSignature},
    {::onc::cellular::kLastConnectedAttachApnProperty, &kCellularApnSignature},
    {::onc::cellular::kLastConnectedDefaultApnProperty, &kCellularApnSignature},
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
    {nullptr}};

const OncFieldSignature network_configuration_fields[] = {
    {::onc::network_config::kCellular, &kCellularSignature},
    {::onc::network_config::kCheckCaptivePortal, &kStringSignature},
    {::onc::network_config::kEthernet, &kEthernetSignature},
    {::onc::network_config::kGUID, &kStringSignature},
    {::onc::network_config::kIPAddressConfigType, &kStringSignature,
     []() { return base::Value(::onc::network_config::kIPConfigTypeDHCP); }},
    {::onc::network_config::kMetered, &kBoolSignature},
    {::onc::network_config::kName, &kStringSignature},
    {::onc::network_config::kNameServersConfigType, &kStringSignature,
     []() { return base::Value(::onc::network_config::kIPConfigTypeDHCP); }},
    {::onc::network_config::kPriority, &kIntegerSignature},
    {::onc::network_config::kProxySettings, &kProxySettingsSignature},
    {::onc::kRecommended, &kRecommendedSignature},
    {::onc::kRemove, &kBoolSignature},
    {::onc::network_config::kStaticIPConfig, &kStaticIPConfigSignature},
    {::onc::network_config::kTether, &kTetherSignature},
    {::onc::network_config::kType, &kStringSignature},
    {::onc::network_config::kVPN, &kVPNSignature},
    {::onc::network_config::kWiFi, &kWiFiSignature},
    {nullptr}};

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
    {::onc::network_config::kTrafficCounterResetTime, &kDoubleSignature},
    {::onc::network_config::kWiFi, &kWiFiWithStateSignature},
    {nullptr}};

const OncFieldSignature global_network_configuration_fields[] = {
    {::onc::global_network_config::kAllowCellularSimLock, &kBoolSignature,
     []() { return base::Value(true); }},
    {::onc::global_network_config::kAllowCellularHotspot, &kBoolSignature,
     []() { return base::Value(true); }},
    {::onc::global_network_config::kAllowOnlyPolicyCellularNetworks,
     &kBoolSignature},
    {::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
     &kBoolSignature},
    {::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
     &kBoolSignature},
    {::onc::global_network_config::kAllowOnlyPolicyWiFiToConnectIfAvailable,
     &kBoolSignature},
    {::onc::global_network_config::kAllowTextMessages, &kStringSignature},
    {/* Deprecated */ ::onc::global_network_config::kBlacklistedHexSSIDs,
     &kStringListSignature},
    {::onc::global_network_config::kBlockedHexSSIDs, &kStringListSignature},
    {::onc::global_network_config::kDisableNetworkTypes, &kStringListSignature},
    {::onc::global_network_config::kRecommendedValuesAreEphemeral,
     &kBoolSignature},
    {::onc::global_network_config::
         kUserCreatedNetworkConfigurationsAreEphemeral,
     &kBoolSignature},
    {::onc::global_network_config::kAllowAPNModification, &kBoolSignature,
     []() { return base::Value(true); }},
    {::onc::global_network_config::kPSIMAdminAssignedAPNIds,
     &kStringListSignature},
    {::onc::global_network_config::kPSIMAdminAssignedAPNs,
     &kCellularApnListSignature},
    {::onc::global_network_config::kDisconnectWiFiOnEthernet,
     &kStringSignature},
    {nullptr}};

const OncFieldSignature certificate_fields[] = {
    {::onc::certificate::kGUID, &kStringSignature},
    {::onc::certificate::kScope, &kScopeSignature},
    {::onc::certificate::kPKCS12, &kStringSignature},
    {::onc::kRemove, &kBoolSignature},
    {::onc::certificate::kTrustBits, &kStringListSignature},
    {::onc::certificate::kType, &kStringSignature},
    {::onc::certificate::kX509, &kStringSignature},
    {nullptr}};

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
    {::onc::toplevel_config::kAdminAPNList, &kCellularApnListSignature},
    {::onc::toplevel_config::kType, &kStringSignature},
    {::onc::encrypted::kCipher, &kStringSignature},
    {::onc::encrypted::kCiphertext, &kStringSignature},
    {::onc::encrypted::kHMAC, &kStringSignature},
    {::onc::encrypted::kHMACMethod, &kStringSignature},
    {::onc::encrypted::kIV, &kStringSignature},
    {::onc::encrypted::kIterations, &kIntegerSignature},
    {::onc::encrypted::kSalt, &kStringSignature},
    {::onc::encrypted::kStretch, &kStringSignature},
    {nullptr}};

const OncFieldSignature eap_subject_alternative_name_match_fields[] = {
    {::onc::eap_subject_alternative_name_match::kType, &kStringSignature},
    {::onc::eap_subject_alternative_name_match::kValue, &kStringSignature},
    {nullptr}};

}  // namespace

const OncValueSignature kRecommendedSignature = {base::Value::Type::LIST,
                                                 nullptr, &kStringSignature};
const OncValueSignature kEAPSignature = {base::Value::Type::DICT, eap_fields,
                                         nullptr};
const OncValueSignature kIssuerSubjectPatternSignature = {
    base::Value::Type::DICT, issuer_subject_pattern_fields, nullptr};
const OncValueSignature kCertificatePatternSignature = {
    base::Value::Type::DICT, certificate_pattern_fields, nullptr};
const OncValueSignature kIPsecSignature = {base::Value::Type::DICT,
                                           ipsec_fields, nullptr};
const OncValueSignature kXAUTHSignature = {base::Value::Type::DICT,
                                           xauth_fields, nullptr};
const OncValueSignature kL2TPSignature = {base::Value::Type::DICT, l2tp_fields,
                                          nullptr};
const OncValueSignature kOpenVPNSignature = {base::Value::Type::DICT,
                                             openvpn_fields, nullptr};
const OncValueSignature kWireGuardSignature = {base::Value::Type::DICT,
                                               wireguard_fields, nullptr};
const OncValueSignature kWireGuardPeerSignature = {
    base::Value::Type::DICT, wireguard_peer_fields, nullptr};
const OncValueSignature kWireGuardPeerListSignature = {
    base::Value::Type::LIST, nullptr, &kWireGuardPeerSignature};
const OncValueSignature kThirdPartyVPNSignature = {
    base::Value::Type::DICT, third_party_vpn_fields, nullptr};
const OncValueSignature kARCVPNSignature = {base::Value::Type::DICT,
                                            arc_vpn_fields, nullptr};
const OncValueSignature kVerifyX509Signature = {base::Value::Type::DICT,
                                                verify_x509_fields, nullptr};
const OncValueSignature kVPNSignature = {base::Value::Type::DICT, vpn_fields,
                                         nullptr};
const OncValueSignature kEthernetSignature = {base::Value::Type::DICT,
                                              ethernet_fields, nullptr};
const OncValueSignature kIPConfigSignature = {base::Value::Type::DICT,
                                              ipconfig_fields, nullptr};
const OncValueSignature kSavedIPConfigSignature = {base::Value::Type::DICT,
                                                   ipconfig_fields, nullptr};
const OncValueSignature kStaticIPConfigSignature = {base::Value::Type::DICT,
                                                    ipconfig_fields, nullptr};
const OncValueSignature kProxyLocationSignature = {
    base::Value::Type::DICT, proxy_location_fields, nullptr};
const OncValueSignature kProxyManualSignature = {base::Value::Type::DICT,
                                                 proxy_manual_fields, nullptr};
const OncValueSignature kProxySettingsSignature = {
    base::Value::Type::DICT, proxy_settings_fields, nullptr};
const OncValueSignature kWiFiSignature = {base::Value::Type::DICT, wifi_fields,
                                          nullptr};
const OncValueSignature kCertificateSignature = {base::Value::Type::DICT,
                                                 certificate_fields, nullptr};
const OncValueSignature kScopeSignature = {base::Value::Type::DICT,
                                           scope_fields, nullptr};
const OncValueSignature kNetworkConfigurationSignature = {
    base::Value::Type::DICT, network_configuration_fields, nullptr};
const OncValueSignature kGlobalNetworkConfigurationSignature = {
    base::Value::Type::DICT, global_network_configuration_fields, nullptr};
const OncValueSignature kCertificateListSignature = {
    base::Value::Type::LIST, nullptr, &kCertificateSignature};
const OncValueSignature kAdminApnListSignature = {
    base::Value::Type::LIST, nullptr, &kCellularApnSignature};
const OncValueSignature kNetworkConfigurationListSignature = {
    base::Value::Type::LIST, nullptr, &kNetworkConfigurationSignature};
const OncValueSignature kToplevelConfigurationSignature = {
    base::Value::Type::DICT, toplevel_configuration_fields, nullptr};

// Derived "ONC with State" signatures.
const OncValueSignature kNetworkWithStateSignature = {
    base::Value::Type::DICT, network_with_state_fields, nullptr,
    &kNetworkConfigurationSignature};
const OncValueSignature kWiFiWithStateSignature = {
    base::Value::Type::DICT, wifi_with_state_fields, nullptr, &kWiFiSignature};
const OncValueSignature kTetherSignature = {base::Value::Type::DICT,
                                            tether_fields, nullptr};
const OncValueSignature kTetherWithStateSignature = {
    base::Value::Type::DICT, tether_with_state_fields, nullptr,
    &kTetherSignature};
const OncValueSignature kCellularSignature = {base::Value::Type::DICT,
                                              cellular_fields, nullptr};
const OncValueSignature kCellularWithStateSignature = {
    base::Value::Type::DICT, cellular_with_state_fields, nullptr,
    &kCellularSignature};
const OncValueSignature kCellularPaymentPortalSignature = {
    base::Value::Type::DICT, cellular_payment_portal_fields, nullptr};
const OncValueSignature kCellularProviderSignature = {
    base::Value::Type::DICT, cellular_provider_fields, nullptr};
const OncValueSignature kCellularApnSignature = {base::Value::Type::DICT,
                                                 cellular_apn_fields, nullptr};
const OncValueSignature kCellularFoundNetworkSignature = {
    base::Value::Type::DICT, cellular_found_network_fields, nullptr};
const OncValueSignature kSIMLockStatusSignature = {
    base::Value::Type::DICT, sim_lock_status_fields, nullptr};
const OncValueSignature kEAPSubjectAlternativeNameMatchSignature = {
    base::Value::Type::DICT, eap_subject_alternative_name_match_fields,
    nullptr};

const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name) {
  if (!signature.fields)
    return nullptr;
  for (const OncFieldSignature* field_signature = signature.fields;
       field_signature->onc_field_name != nullptr; ++field_signature) {
    if (onc_field_name == field_signature->onc_field_name)
      return field_signature;
  }
  if (signature.base_signature)
    return GetFieldSignature(*signature.base_signature, onc_field_name);
  return nullptr;
}

namespace {

struct CredentialEntry {
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const OncValueSignature* value_signature;
  const char* field_name;
};

const CredentialEntry credentials[] = {
    {&kEAPSignature, ::onc::eap::kPassword},
    {&kIPsecSignature, ::onc::ipsec::kPSK},
    {&kXAUTHSignature, ::onc::vpn::kPassword},
    {&kL2TPSignature, ::onc::vpn::kPassword},
    {&kOpenVPNSignature, ::onc::vpn::kPassword},
    {&kOpenVPNSignature, ::onc::openvpn::kTLSAuthContents},
    {&kWireGuardSignature, ::onc::wireguard::kPrivateKey},
    {&kWireGuardPeerSignature, ::onc::wireguard::kPresharedKey},
    {&kWiFiSignature, ::onc::wifi::kPassphrase},
    {&kCellularApnSignature, ::onc::cellular_apn::kPassword},
    // While not really a credential, PKCS12 blobs may contain unencrypted
    // private keys.
    {&kCertificateSignature, ::onc::certificate::kPKCS12},
    {nullptr}};

}  // namespace

bool FieldIsCredential(const OncValueSignature& signature,
                       const std::string& onc_field_name) {
  for (const CredentialEntry* entry = credentials;
       entry->value_signature != nullptr; ++entry) {
    if (&signature == entry->value_signature &&
        onc_field_name == entry->field_name) {
      return true;
    }
  }
  return false;
}

}  // namespace onc
}  // namespace chromeos
