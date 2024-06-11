// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ONC_ONC_CONSTANTS_H_
#define COMPONENTS_ONC_ONC_CONSTANTS_H_

#include <string>

#include "base/component_export.h"

// Constants for ONC properties.
namespace onc {

// Indicates from which source an ONC blob comes from.
enum ONCSource {
  ONC_SOURCE_UNKNOWN,
  ONC_SOURCE_NONE,
  ONC_SOURCE_USER_IMPORT,
  ONC_SOURCE_DEVICE_POLICY,
  ONC_SOURCE_USER_POLICY,
};

// These keys are used to augment the dictionary resulting from merging the
// different settings and policies.

// The setting that Shill declared to be using. For example, if no policy and no
// user setting exists, Shill might still report a property like network
// security options or a SSID.
COMPONENT_EXPORT(ONC) extern const char kAugmentationActiveSetting[];
// The one of different setting sources (user/device policy, user/shared
// settings) that has highest priority over the others.
COMPONENT_EXPORT(ONC) extern const char kAugmentationEffectiveSetting[];
COMPONENT_EXPORT(ONC) extern const char kAugmentationUserPolicy[];
COMPONENT_EXPORT(ONC) extern const char kAugmentationDevicePolicy[];
COMPONENT_EXPORT(ONC) extern const char kAugmentationUserSetting[];
COMPONENT_EXPORT(ONC) extern const char kAugmentationSharedSetting[];
COMPONENT_EXPORT(ONC) extern const char kAugmentationUserEditable[];
COMPONENT_EXPORT(ONC) extern const char kAugmentationDeviceEditable[];

// Special key for indicating that the Effective value is the Active value
// and was set by an extension. Used for ProxySettings.
COMPONENT_EXPORT(ONC) extern const char kAugmentationActiveExtension[];

// Common keys/values.
COMPONENT_EXPORT(ONC) extern const char kRecommended[];
COMPONENT_EXPORT(ONC) extern const char kRemove[];

// Top Level Configuration
namespace toplevel_config {
COMPONENT_EXPORT(ONC) extern const char kAdminAPNList[];
COMPONENT_EXPORT(ONC) extern const char kCertificates[];
COMPONENT_EXPORT(ONC) extern const char kEncryptedConfiguration[];
COMPONENT_EXPORT(ONC) extern const char kNetworkConfigurations[];
COMPONENT_EXPORT(ONC) extern const char kGlobalNetworkConfiguration[];
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kUnencryptedConfiguration[];
}  // namespace toplevel_config

// NetworkConfiguration.
namespace network_config {
COMPONENT_EXPORT(ONC) extern const char kCellular[];
COMPONENT_EXPORT(ONC) extern const char kCheckCaptivePortal[];
COMPONENT_EXPORT(ONC) extern const char kDevice[];
COMPONENT_EXPORT(ONC) extern const char kEthernet[];
COMPONENT_EXPORT(ONC) extern const char kGUID[];
COMPONENT_EXPORT(ONC) extern const char kIPAddressConfigType[];
COMPONENT_EXPORT(ONC) extern const char kIPConfigs[];
COMPONENT_EXPORT(ONC) extern const char kIPConfigTypeDHCP[];
COMPONENT_EXPORT(ONC) extern const char kIPConfigTypeStatic[];
COMPONENT_EXPORT(ONC) extern const char kSavedIPConfig[];
COMPONENT_EXPORT(ONC) extern const char kStaticIPConfig[];
COMPONENT_EXPORT(ONC) extern const char kMacAddress[];
COMPONENT_EXPORT(ONC) extern const char kMetered[];
COMPONENT_EXPORT(ONC) extern const char kNameServersConfigType[];
COMPONENT_EXPORT(ONC) extern const char kName[];
COMPONENT_EXPORT(ONC) extern const char kPriority[];
COMPONENT_EXPORT(ONC) extern const char kProxySettings[];
COMPONENT_EXPORT(ONC) extern const char kSource[];
COMPONENT_EXPORT(ONC) extern const char kSourceDevice[];
COMPONENT_EXPORT(ONC) extern const char kSourceDevicePolicy[];
COMPONENT_EXPORT(ONC) extern const char kSourceNone[];
COMPONENT_EXPORT(ONC) extern const char kSourceUser[];
COMPONENT_EXPORT(ONC) extern const char kSourceUserPolicy[];
COMPONENT_EXPORT(ONC) extern const char kConnectionState[];
COMPONENT_EXPORT(ONC) extern const char kRestrictedConnectivity[];
COMPONENT_EXPORT(ONC) extern const char kConnectable[];
COMPONENT_EXPORT(ONC) extern const char kErrorState[];
COMPONENT_EXPORT(ONC) extern const char kTether[];
COMPONENT_EXPORT(ONC) extern const char kTrafficCounterResetTime[];
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kVPN[];
COMPONENT_EXPORT(ONC) extern const char kWiFi[];
COMPONENT_EXPORT(ONC) extern const char kWimaxDeprecated[];

COMPONENT_EXPORT(ONC)
extern std::string CellularProperty(const std::string& property);
COMPONENT_EXPORT(ONC)
extern std::string TetherProperty(const std::string& property);
COMPONENT_EXPORT(ONC)
extern std::string VpnProperty(const std::string& property);
COMPONENT_EXPORT(ONC)
extern std::string WifiProperty(const std::string& property);

}  // namespace network_config

namespace network_type {
COMPONENT_EXPORT(ONC) extern const char kCellular[];
COMPONENT_EXPORT(ONC) extern const char kEthernet[];
COMPONENT_EXPORT(ONC) extern const char kTether[];
COMPONENT_EXPORT(ONC) extern const char kVPN[];
COMPONENT_EXPORT(ONC) extern const char kWiFi[];
COMPONENT_EXPORT(ONC) extern const char kWimaxDeprecated[];
// Patterns matching multiple types, not part of the ONC spec.
COMPONENT_EXPORT(ONC) extern const char kAllTypes[];
COMPONENT_EXPORT(ONC) extern const char kWireless[];
}  // namespace network_type

namespace check_captive_portal {
COMPONENT_EXPORT(ONC) extern const char kFalse[];
COMPONENT_EXPORT(ONC) extern const char kHTTPOnly[];
COMPONENT_EXPORT(ONC) extern const char kTrue[];
}  // namespace check_captive_portal

namespace cellular {
COMPONENT_EXPORT(ONC) extern const char kActivationState[];
COMPONENT_EXPORT(ONC) extern const char kActivated[];
COMPONENT_EXPORT(ONC) extern const char kActivating[];
COMPONENT_EXPORT(ONC) extern const char kAutoConnect[];
COMPONENT_EXPORT(ONC) extern const char kNotActivated[];
COMPONENT_EXPORT(ONC) extern const char kPartiallyActivated[];
COMPONENT_EXPORT(ONC) extern const char kActivationType[];
COMPONENT_EXPORT(ONC) extern const char kAdminAssignedAPNIds[];
COMPONENT_EXPORT(ONC) extern const char kAllowRoaming[];
COMPONENT_EXPORT(ONC) extern const char kAPN[];
COMPONENT_EXPORT(ONC) extern const char kAPNList[];
COMPONENT_EXPORT(ONC) extern const char kCarrier[];
COMPONENT_EXPORT(ONC) extern const char kCustomAPNList[];
COMPONENT_EXPORT(ONC) extern const char kESN[];
COMPONENT_EXPORT(ONC) extern const char kFamily[];
COMPONENT_EXPORT(ONC) extern const char kFirmwareRevision[];
COMPONENT_EXPORT(ONC) extern const char kFoundNetworks[];
COMPONENT_EXPORT(ONC) extern const char kHardwareRevision[];
COMPONENT_EXPORT(ONC) extern const char kHomeProvider[];
COMPONENT_EXPORT(ONC) extern const char kEID[];
COMPONENT_EXPORT(ONC) extern const char kICCID[];
COMPONENT_EXPORT(ONC) extern const char kIMEI[];
COMPONENT_EXPORT(ONC) extern const char kIMSI[];
COMPONENT_EXPORT(ONC) extern const char kLastConnectedAttachApnProperty[];
COMPONENT_EXPORT(ONC) extern const char kLastConnectedDefaultApnProperty[];
COMPONENT_EXPORT(ONC) extern const char kLastGoodAPN[];
COMPONENT_EXPORT(ONC) extern const char kManufacturer[];
COMPONENT_EXPORT(ONC) extern const char kMDN[];
COMPONENT_EXPORT(ONC) extern const char kMEID[];
COMPONENT_EXPORT(ONC) extern const char kMIN[];
COMPONENT_EXPORT(ONC) extern const char kModelID[];
COMPONENT_EXPORT(ONC) extern const char kNetworkTechnology[];
COMPONENT_EXPORT(ONC) extern const char kPaymentPortal[];
COMPONENT_EXPORT(ONC) extern const char kRoamingHome[];
COMPONENT_EXPORT(ONC) extern const char kRoamingRequired[];
COMPONENT_EXPORT(ONC) extern const char kRoamingRoaming[];
COMPONENT_EXPORT(ONC) extern const char kRoamingState[];
COMPONENT_EXPORT(ONC) extern const char kScanning[];
COMPONENT_EXPORT(ONC) extern const char kServingOperator[];
COMPONENT_EXPORT(ONC) extern const char kSignalStrength[];
COMPONENT_EXPORT(ONC) extern const char kSIMLockStatus[];
COMPONENT_EXPORT(ONC) extern const char kSIMPresent[];
COMPONENT_EXPORT(ONC) extern const char kSMDPAddress[];
COMPONENT_EXPORT(ONC) extern const char kSMDSAddress[];
COMPONENT_EXPORT(ONC) extern const char kSupportNetworkScan[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyCdma1Xrtt[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyEdge[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyEvdo[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyGprs[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyGsm[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyHspa[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyHspaPlus[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyLte[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyLteAdvanced[];
COMPONENT_EXPORT(ONC) extern const char kTechnologyUmts[];
COMPONENT_EXPORT(ONC) extern const char kTechnology5gNr[];
COMPONENT_EXPORT(ONC) extern const char kTextMessagesAllow[];
COMPONENT_EXPORT(ONC) extern const char kTextMessagesSuppress[];
COMPONENT_EXPORT(ONC) extern const char kTextMessagesUnset[];
}  // namespace cellular

namespace cellular_provider {
COMPONENT_EXPORT(ONC) extern const char kCode[];
COMPONENT_EXPORT(ONC) extern const char kCountry[];
COMPONENT_EXPORT(ONC) extern const char kName[];
}  // namespace cellular_provider

namespace cellular_apn {
COMPONENT_EXPORT(ONC) extern const char kAccessPointName[];
COMPONENT_EXPORT(ONC) extern const char kName[];
COMPONENT_EXPORT(ONC) extern const char kUsername[];
COMPONENT_EXPORT(ONC) extern const char kPassword[];
COMPONENT_EXPORT(ONC) extern const char kAuthentication[];
COMPONENT_EXPORT(ONC) extern const char kLocalizedName[];
COMPONENT_EXPORT(ONC) extern const char kLanguage[];
COMPONENT_EXPORT(ONC) extern const char kAttach[];
COMPONENT_EXPORT(ONC) extern const char kId[];
COMPONENT_EXPORT(ONC) extern const char kState[];
COMPONENT_EXPORT(ONC) extern const char kStateEnabled[];
COMPONENT_EXPORT(ONC) extern const char kStateDisabled[];
COMPONENT_EXPORT(ONC) extern const char kAuthenticationAutomatic[];
COMPONENT_EXPORT(ONC) extern const char kAuthenticationPap[];
COMPONENT_EXPORT(ONC) extern const char kAuthenticationChap[];
COMPONENT_EXPORT(ONC) extern const char kIpType[];
COMPONENT_EXPORT(ONC) extern const char kIpTypeAutomatic[];
COMPONENT_EXPORT(ONC) extern const char kIpTypeIpv4[];
COMPONENT_EXPORT(ONC) extern const char kIpTypeIpv6[];
COMPONENT_EXPORT(ONC) extern const char kIpTypeIpv4Ipv6[];
COMPONENT_EXPORT(ONC) extern const char kApnTypes[];
COMPONENT_EXPORT(ONC) extern const char kApnTypeDefault[];
COMPONENT_EXPORT(ONC) extern const char kApnTypeAttach[];
COMPONENT_EXPORT(ONC) extern const char kApnTypeTether[];
COMPONENT_EXPORT(ONC) extern const char kSource[];
COMPONENT_EXPORT(ONC) extern const char kSourceUi[];
COMPONENT_EXPORT(ONC) extern const char kSourceAdmin[];
COMPONENT_EXPORT(ONC) extern const char kSourceModb[];
COMPONENT_EXPORT(ONC) extern const char kSourceModem[];
}  // namespace cellular_apn

namespace cellular_found_network {
COMPONENT_EXPORT(ONC) extern const char kStatus[];
COMPONENT_EXPORT(ONC) extern const char kNetworkId[];
COMPONENT_EXPORT(ONC) extern const char kShortName[];
COMPONENT_EXPORT(ONC) extern const char kLongName[];
COMPONENT_EXPORT(ONC) extern const char kTechnology[];
}  // namespace cellular_found_network

namespace cellular_payment_portal {
COMPONENT_EXPORT(ONC) extern const char kMethod[];
COMPONENT_EXPORT(ONC) extern const char kPostData[];
COMPONENT_EXPORT(ONC) extern const char kUrl[];
}  // namespace cellular_payment_portal

namespace sim_lock_status {
COMPONENT_EXPORT(ONC) extern const char kLockEnabled[];
COMPONENT_EXPORT(ONC) extern const char kLockType[];
COMPONENT_EXPORT(ONC) extern const char kRetriesLeft[];
}  // namespace sim_lock_status

namespace connection_state {
COMPONENT_EXPORT(ONC) extern const char kConnected[];
COMPONENT_EXPORT(ONC) extern const char kConnecting[];
COMPONENT_EXPORT(ONC) extern const char kNotConnected[];
}  // namespace connection_state

namespace ipconfig {
COMPONENT_EXPORT(ONC) extern const char kGateway[];
COMPONENT_EXPORT(ONC) extern const char kIPAddress[];
COMPONENT_EXPORT(ONC) extern const char kIPv4[];
COMPONENT_EXPORT(ONC) extern const char kIPv6[];
COMPONENT_EXPORT(ONC) extern const char kNameServers[];
COMPONENT_EXPORT(ONC) extern const char kRoutingPrefix[];
COMPONENT_EXPORT(ONC) extern const char kSearchDomains[];
COMPONENT_EXPORT(ONC) extern const char kIncludedRoutes[];
COMPONENT_EXPORT(ONC) extern const char kExcludedRoutes[];
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kWebProxyAutoDiscoveryUrl[];
COMPONENT_EXPORT(ONC) extern const char kMTU[];
}  // namespace ipconfig

namespace ethernet {
COMPONENT_EXPORT(ONC) extern const char kAuthentication[];
COMPONENT_EXPORT(ONC) extern const char kAuthenticationNone[];
COMPONENT_EXPORT(ONC) extern const char kEAP[];
COMPONENT_EXPORT(ONC) extern const char k8021X[];
}  // namespace ethernet

namespace tether {
COMPONENT_EXPORT(ONC) extern const char kBatteryPercentage[];
COMPONENT_EXPORT(ONC) extern const char kCarrier[];
COMPONENT_EXPORT(ONC) extern const char kHasConnectedToHost[];
COMPONENT_EXPORT(ONC) extern const char kSignalStrength[];
}  // namespace tether

namespace wifi {
COMPONENT_EXPORT(ONC) extern const char kAllowGatewayARPPolling[];
COMPONENT_EXPORT(ONC) extern const char kAutoConnect[];
COMPONENT_EXPORT(ONC) extern const char kBSSID[];
COMPONENT_EXPORT(ONC) extern const char kBSSIDAllowlist[];
COMPONENT_EXPORT(ONC) extern const char kBSSIDRequested[];
COMPONENT_EXPORT(ONC) extern const char kEAP[];
COMPONENT_EXPORT(ONC) extern const char kFrequency[];
COMPONENT_EXPORT(ONC) extern const char kFrequencyList[];
COMPONENT_EXPORT(ONC) extern const char kHexSSID[];
COMPONENT_EXPORT(ONC) extern const char kHiddenSSID[];
COMPONENT_EXPORT(ONC) extern const char kPassphrase[];
COMPONENT_EXPORT(ONC) extern const char kSSID[];
COMPONENT_EXPORT(ONC) extern const char kSecurity[];
COMPONENT_EXPORT(ONC) extern const char kSecurityNone[];
COMPONENT_EXPORT(ONC) extern const char kSignalStrength[];
COMPONENT_EXPORT(ONC) extern const char kSignalStrengthRssi[];
COMPONENT_EXPORT(ONC) extern const char kWEP_PSK[];
COMPONENT_EXPORT(ONC) extern const char kWEP_8021X[];
COMPONENT_EXPORT(ONC) extern const char kWPA_PSK[];
COMPONENT_EXPORT(ONC) extern const char kWPA2_PSK[];
COMPONENT_EXPORT(ONC) extern const char kWPA_EAP[];
COMPONENT_EXPORT(ONC) extern const char kPasspointId[];
COMPONENT_EXPORT(ONC) extern const char kPasspointMatchType[];
}  // namespace wifi

namespace wimax_deprecated {
COMPONENT_EXPORT(ONC) extern const char kAutoConnect[];
COMPONENT_EXPORT(ONC) extern const char kEAP[];
}  // namespace wimax_deprecated

namespace client_cert {
COMPONENT_EXPORT(ONC) extern const char kClientCertProvisioningProfileId[];
COMPONENT_EXPORT(ONC) extern const char kClientCertPattern[];
COMPONENT_EXPORT(ONC) extern const char kClientCertPKCS11Id[];
COMPONENT_EXPORT(ONC) extern const char kClientCertRef[];
COMPONENT_EXPORT(ONC) extern const char kClientCertType[];
COMPONENT_EXPORT(ONC) extern const char kClientCertTypeNone[];
COMPONENT_EXPORT(ONC) extern const char kCommonName[];
COMPONENT_EXPORT(ONC) extern const char kEmailAddress[];
COMPONENT_EXPORT(ONC) extern const char kEnrollmentURI[];
COMPONENT_EXPORT(ONC) extern const char kIssuerCARef[];
COMPONENT_EXPORT(ONC) extern const char kIssuerCAPEMs[];
COMPONENT_EXPORT(ONC) extern const char kIssuer[];
COMPONENT_EXPORT(ONC) extern const char kLocality[];
COMPONENT_EXPORT(ONC) extern const char kOrganization[];
COMPONENT_EXPORT(ONC) extern const char kOrganizationalUnit[];
COMPONENT_EXPORT(ONC) extern const char kPattern[];
COMPONENT_EXPORT(ONC) extern const char kProvisioningProfileId[];
COMPONENT_EXPORT(ONC) extern const char kPKCS11Id[];
COMPONENT_EXPORT(ONC) extern const char kRef[];
COMPONENT_EXPORT(ONC) extern const char kSubject[];
}  // namespace client_cert

namespace certificate {
COMPONENT_EXPORT(ONC) extern const char kAuthority[];
COMPONENT_EXPORT(ONC) extern const char kClient[];
COMPONENT_EXPORT(ONC) extern const char kGUID[];
COMPONENT_EXPORT(ONC) extern const char kPKCS12[];
COMPONENT_EXPORT(ONC) extern const char kScope[];
COMPONENT_EXPORT(ONC) extern const char kServer[];
COMPONENT_EXPORT(ONC) extern const char kTrustBits[];
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kWeb[];
COMPONENT_EXPORT(ONC) extern const char kX509[];
}  // namespace certificate

namespace scope {
COMPONENT_EXPORT(ONC) extern const char kDefault[];
COMPONENT_EXPORT(ONC) extern const char kExtension[];
COMPONENT_EXPORT(ONC) extern const char kId[];
COMPONENT_EXPORT(ONC) extern const char kType[];
}  // namespace scope

namespace encrypted {
COMPONENT_EXPORT(ONC) extern const char kAES256[];
COMPONENT_EXPORT(ONC) extern const char kCipher[];
COMPONENT_EXPORT(ONC) extern const char kCiphertext[];
COMPONENT_EXPORT(ONC) extern const char kHMACMethod[];
COMPONENT_EXPORT(ONC) extern const char kHMAC[];
COMPONENT_EXPORT(ONC) extern const char kIV[];
COMPONENT_EXPORT(ONC) extern const char kIterations[];
COMPONENT_EXPORT(ONC) extern const char kPBKDF2[];
COMPONENT_EXPORT(ONC) extern const char kSHA1[];
COMPONENT_EXPORT(ONC) extern const char kSalt[];
COMPONENT_EXPORT(ONC) extern const char kStretch[];
}  // namespace encrypted

namespace eap {
COMPONENT_EXPORT(ONC) extern const char kAnonymousIdentity[];
COMPONENT_EXPORT(ONC) extern const char kAutomatic[];
COMPONENT_EXPORT(ONC) extern const char kCHAP[];
COMPONENT_EXPORT(ONC) extern const char kDomainSuffixMatch[];
COMPONENT_EXPORT(ONC) extern const char kEAP_AKA[];
COMPONENT_EXPORT(ONC) extern const char kEAP_FAST[];
COMPONENT_EXPORT(ONC) extern const char kEAP_SIM[];
COMPONENT_EXPORT(ONC) extern const char kEAP_TLS[];
COMPONENT_EXPORT(ONC) extern const char kEAP_TTLS[];
COMPONENT_EXPORT(ONC) extern const char kGTC[];
COMPONENT_EXPORT(ONC) extern const char kIdentity[];
COMPONENT_EXPORT(ONC) extern const char kInner[];
COMPONENT_EXPORT(ONC) extern const char kLEAP[];
COMPONENT_EXPORT(ONC) extern const char kMD5[];
COMPONENT_EXPORT(ONC) extern const char kMSCHAP[];
COMPONENT_EXPORT(ONC) extern const char kMSCHAPv2[];
COMPONENT_EXPORT(ONC) extern const char kOuter[];
COMPONENT_EXPORT(ONC) extern const char kPAP[];
COMPONENT_EXPORT(ONC) extern const char kPEAP[];
COMPONENT_EXPORT(ONC) extern const char kPassword[];
COMPONENT_EXPORT(ONC) extern const char kSaveCredentials[];
COMPONENT_EXPORT(ONC) extern const char kServerCAPEMs[];
COMPONENT_EXPORT(ONC) extern const char kServerCARef[];
COMPONENT_EXPORT(ONC) extern const char kServerCARefs[];
COMPONENT_EXPORT(ONC) extern const char kSubjectMatch[];
COMPONENT_EXPORT(ONC) extern const char kSubjectAlternativeNameMatch[];
COMPONENT_EXPORT(ONC) extern const char kTLSVersionMax[];
COMPONENT_EXPORT(ONC) extern const char kUseSystemCAs[];
COMPONENT_EXPORT(ONC) extern const char kUseProactiveKeyCaching[];
}  // namespace eap

namespace eap_subject_alternative_name_match {
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kValue[];
COMPONENT_EXPORT(ONC) extern const char kEMAIL[];
COMPONENT_EXPORT(ONC) extern const char kDNS[];
COMPONENT_EXPORT(ONC) extern const char kURI[];
}  // namespace eap_subject_alternative_name_match

namespace vpn {
COMPONENT_EXPORT(ONC) extern const char kArcVpn[];
COMPONENT_EXPORT(ONC) extern const char kAutoConnect[];
COMPONENT_EXPORT(ONC) extern const char kHost[];
COMPONENT_EXPORT(ONC) extern const char kIPsec[];
COMPONENT_EXPORT(ONC) extern const char kL2TP[];
COMPONENT_EXPORT(ONC) extern const char kOpenVPN[];
COMPONENT_EXPORT(ONC) extern const char kPassword[];
COMPONENT_EXPORT(ONC) extern const char kSaveCredentials[];
COMPONENT_EXPORT(ONC) extern const char kThirdPartyVpn[];
COMPONENT_EXPORT(ONC) extern const char kTypeL2TP_IPsec[];
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kUsername[];
COMPONENT_EXPORT(ONC) extern const char kWireGuard[];
}  // namespace vpn

namespace ipsec {
COMPONENT_EXPORT(ONC) extern const char kAuthenticationType[];
COMPONENT_EXPORT(ONC) extern const char kCert[];
COMPONENT_EXPORT(ONC) extern const char kEAP[];
COMPONENT_EXPORT(ONC) extern const char kGroup[];
COMPONENT_EXPORT(ONC) extern const char kIKEVersion[];
COMPONENT_EXPORT(ONC) extern const char kLocalIdentity[];
COMPONENT_EXPORT(ONC) extern const char kPSK[];
COMPONENT_EXPORT(ONC) extern const char kRemoteIdentity[];
COMPONENT_EXPORT(ONC) extern const char kServerCAPEMs[];
COMPONENT_EXPORT(ONC) extern const char kServerCARef[];
COMPONENT_EXPORT(ONC) extern const char kServerCARefs[];
COMPONENT_EXPORT(ONC) extern const char kXAUTH[];
}  // namespace ipsec

namespace l2tp {
COMPONENT_EXPORT(ONC) extern const char kLcpEchoDisabled[];
COMPONENT_EXPORT(ONC) extern const char kPassword[];
COMPONENT_EXPORT(ONC) extern const char kSaveCredentials[];
COMPONENT_EXPORT(ONC) extern const char kUsername[];
}  // namespace l2tp

namespace openvpn {
COMPONENT_EXPORT(ONC) extern const char kAuthNoCache[];
COMPONENT_EXPORT(ONC) extern const char kAuthRetry[];
COMPONENT_EXPORT(ONC) extern const char kAuth[];
COMPONENT_EXPORT(ONC) extern const char kCipher[];
COMPONENT_EXPORT(ONC) extern const char kCompLZO[];
COMPONENT_EXPORT(ONC) extern const char kCompNoAdapt[];
COMPONENT_EXPORT(ONC) extern const char kCompressionAlgorithm[];
COMPONENT_EXPORT(ONC) extern const char kExtraHosts[];
COMPONENT_EXPORT(ONC) extern const char kIgnoreDefaultRoute[];
COMPONENT_EXPORT(ONC) extern const char kInteract[];
COMPONENT_EXPORT(ONC) extern const char kKeyDirection[];
COMPONENT_EXPORT(ONC) extern const char kNoInteract[];
COMPONENT_EXPORT(ONC) extern const char kNone[];
COMPONENT_EXPORT(ONC) extern const char kNsCertType[];
COMPONENT_EXPORT(ONC) extern const char kOTP[];
COMPONENT_EXPORT(ONC) extern const char kPassword[];
COMPONENT_EXPORT(ONC) extern const char kPort[];
COMPONENT_EXPORT(ONC) extern const char kProto[];
COMPONENT_EXPORT(ONC) extern const char kPushPeerInfo[];
COMPONENT_EXPORT(ONC) extern const char kRemoteCertEKU[];
COMPONENT_EXPORT(ONC) extern const char kRemoteCertKU[];
COMPONENT_EXPORT(ONC) extern const char kRemoteCertTLS[];
COMPONENT_EXPORT(ONC) extern const char kRenegSec[];
COMPONENT_EXPORT(ONC) extern const char kServerCAPEMs[];
COMPONENT_EXPORT(ONC) extern const char kServerCARef[];
COMPONENT_EXPORT(ONC) extern const char kServerCARefs[];
COMPONENT_EXPORT(ONC) extern const char kServerCertPEM[];
COMPONENT_EXPORT(ONC) extern const char kServerCertRef[];
COMPONENT_EXPORT(ONC) extern const char kServerPollTimeout[];
COMPONENT_EXPORT(ONC) extern const char kServer[];
COMPONENT_EXPORT(ONC) extern const char kShaper[];
COMPONENT_EXPORT(ONC) extern const char kStaticChallenge[];
COMPONENT_EXPORT(ONC) extern const char kTLSAuthContents[];
COMPONENT_EXPORT(ONC) extern const char kTLSRemote[];
COMPONENT_EXPORT(ONC) extern const char kTLSVersionMin[];
COMPONENT_EXPORT(ONC) extern const char kUserAuthenticationType[];
COMPONENT_EXPORT(ONC) extern const char kVerb[];
COMPONENT_EXPORT(ONC) extern const char kVerifyHash[];
COMPONENT_EXPORT(ONC) extern const char kVerifyX509[];
}  // namespace openvpn

namespace wireguard {
COMPONENT_EXPORT(ONC) extern const char kAllowedIPs[];
COMPONENT_EXPORT(ONC) extern const char kEndpoint[];
COMPONENT_EXPORT(ONC) extern const char kIPAddresses[];
COMPONENT_EXPORT(ONC) extern const char kPeers[];
COMPONENT_EXPORT(ONC) extern const char kPersistentKeepalive[];
COMPONENT_EXPORT(ONC) extern const char kPresharedKey[];
COMPONENT_EXPORT(ONC) extern const char kPrivateKey[];
COMPONENT_EXPORT(ONC) extern const char kPublicKey[];
}  // namespace wireguard

namespace openvpn_compression_algorithm {
COMPONENT_EXPORT(ONC) extern const char kFramingOnly[];
COMPONENT_EXPORT(ONC) extern const char kLz4[];
COMPONENT_EXPORT(ONC) extern const char kLz4V2[];
COMPONENT_EXPORT(ONC) extern const char kLzo[];
COMPONENT_EXPORT(ONC) extern const char kNone[];
}  // namespace openvpn_compression_algorithm

namespace openvpn_user_auth_type {
COMPONENT_EXPORT(ONC) extern const char kNone[];
COMPONENT_EXPORT(ONC) extern const char kOTP[];
COMPONENT_EXPORT(ONC) extern const char kPassword[];
COMPONENT_EXPORT(ONC) extern const char kPasswordAndOTP[];
}  // namespace openvpn_user_auth_type

namespace third_party_vpn {
COMPONENT_EXPORT(ONC) extern const char kExtensionID[];
COMPONENT_EXPORT(ONC) extern const char kProviderName[];
}  // namespace third_party_vpn

namespace arc_vpn {
COMPONENT_EXPORT(ONC) extern const char kTunnelChrome[];
}  // namespace arc_vpn

namespace verify_x509 {
COMPONENT_EXPORT(ONC) extern const char kName[];
COMPONENT_EXPORT(ONC) extern const char kType[];

namespace types {
COMPONENT_EXPORT(ONC) extern const char kName[];
COMPONENT_EXPORT(ONC) extern const char kNamePrefix[];
COMPONENT_EXPORT(ONC) extern const char kSubject[];
}  // namespace types
}  // namespace verify_x509

namespace substitutes {
COMPONENT_EXPORT(ONC) extern const char kLoginEmail[];
COMPONENT_EXPORT(ONC) extern const char kLoginID[];
COMPONENT_EXPORT(ONC) extern const char kCertSANEmail[];
COMPONENT_EXPORT(ONC) extern const char kCertSANUPN[];
COMPONENT_EXPORT(ONC) extern const char kCertSubjectCommonName[];
COMPONENT_EXPORT(ONC) extern const char kDeviceSerialNumber[];
COMPONENT_EXPORT(ONC) extern const char kDeviceAssetId[];
COMPONENT_EXPORT(ONC) extern const char kPasswordPlaceholderVerbatim[];
}  // namespace substitutes

namespace proxy {
COMPONENT_EXPORT(ONC) extern const char kDirect[];
COMPONENT_EXPORT(ONC) extern const char kExcludeDomains[];
COMPONENT_EXPORT(ONC) extern const char kFtp[];
COMPONENT_EXPORT(ONC) extern const char kHost[];
COMPONENT_EXPORT(ONC) extern const char kHttp[];
COMPONENT_EXPORT(ONC) extern const char kHttps[];
COMPONENT_EXPORT(ONC) extern const char kManual[];
COMPONENT_EXPORT(ONC) extern const char kPAC[];
COMPONENT_EXPORT(ONC) extern const char kPort[];
COMPONENT_EXPORT(ONC) extern const char kSocks[];
COMPONENT_EXPORT(ONC) extern const char kType[];
COMPONENT_EXPORT(ONC) extern const char kWPAD[];
}  // namespace proxy

namespace global_network_config {
COMPONENT_EXPORT(ONC) extern const char kAllowAPNModification[];
COMPONENT_EXPORT(ONC) extern const char kAllowCellularSimLock[];
COMPONENT_EXPORT(ONC) extern const char kAllowCellularHotspot[];
COMPONENT_EXPORT(ONC) extern const char kAllowOnlyPolicyCellularNetworks[];
COMPONENT_EXPORT(ONC) extern const char kAllowOnlyPolicyNetworksToAutoconnect[];
COMPONENT_EXPORT(ONC) extern const char* const kAllowOnlyPolicyWiFiToConnect;
COMPONENT_EXPORT(ONC)
extern const char* const kAllowOnlyPolicyWiFiToConnectIfAvailable;
COMPONENT_EXPORT(ONC)
extern const char* const kAllowTextMessages;
COMPONENT_EXPORT(ONC) extern const char kBlacklistedHexSSIDs[];  // Deprecated
COMPONENT_EXPORT(ONC) extern const char kBlockedHexSSIDs[];
COMPONENT_EXPORT(ONC) extern const char kDisableNetworkTypes[];
COMPONENT_EXPORT(ONC) extern const char kRecommendedValuesAreEphemeral[];
COMPONENT_EXPORT(ONC) extern const char kPSIMAdminAssignedAPNIds[];
COMPONENT_EXPORT(ONC) extern const char kPSIMAdminAssignedAPNs[];
COMPONENT_EXPORT(ONC)
extern const char kUserCreatedNetworkConfigurationsAreEphemeral[];
COMPONENT_EXPORT(ONC) extern const char kDisconnectWiFiOnEthernet[];
COMPONENT_EXPORT(ONC)
extern const char kDisconnectWiFiOnEthernetWhenConnected[];
COMPONENT_EXPORT(ONC) extern const char kDisconnectWiFiOnEthernetWhenOnline[];
}  // namespace global_network_config

namespace device_state {
COMPONENT_EXPORT(ONC) extern const char kUninitialized[];
COMPONENT_EXPORT(ONC) extern const char kDisabled[];
COMPONENT_EXPORT(ONC) extern const char kEnabling[];
COMPONENT_EXPORT(ONC) extern const char kEnabled[];
}  // namespace device_state

}  // namespace onc

#endif  // COMPONENTS_ONC_ONC_CONSTANTS_H_
