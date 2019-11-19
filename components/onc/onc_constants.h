// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ONC_ONC_CONSTANTS_H_
#define COMPONENTS_ONC_ONC_CONSTANTS_H_

#include <string>

#include "components/onc/onc_export.h"

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
ONC_EXPORT extern const char kAugmentationActiveSetting[];
// The one of different setting sources (user/device policy, user/shared
// settings) that has highest priority over the others.
ONC_EXPORT extern const char kAugmentationEffectiveSetting[];
ONC_EXPORT extern const char kAugmentationUserPolicy[];
ONC_EXPORT extern const char kAugmentationDevicePolicy[];
ONC_EXPORT extern const char kAugmentationUserSetting[];
ONC_EXPORT extern const char kAugmentationSharedSetting[];
ONC_EXPORT extern const char kAugmentationUserEditable[];
ONC_EXPORT extern const char kAugmentationDeviceEditable[];

// Special key for indicating that the Effective value is the Active value
// and was set by an extension. Used for ProxySettings.
ONC_EXPORT extern const char kAugmentationActiveExtension[];

// Common keys/values.
ONC_EXPORT extern const char kRecommended[];
ONC_EXPORT extern const char kRemove[];

// Top Level Configuration
namespace toplevel_config {
ONC_EXPORT extern const char kCertificates[];
ONC_EXPORT extern const char kEncryptedConfiguration[];
ONC_EXPORT extern const char kNetworkConfigurations[];
ONC_EXPORT extern const char kGlobalNetworkConfiguration[];
ONC_EXPORT extern const char kType[];
ONC_EXPORT extern const char kUnencryptedConfiguration[];
}  // namespace toplevel_config

// NetworkConfiguration.
namespace network_config {
ONC_EXPORT extern const char kCellular[];
ONC_EXPORT extern const char kDevice[];
ONC_EXPORT extern const char kEthernet[];
ONC_EXPORT extern const char kGUID[];
ONC_EXPORT extern const char kIPAddressConfigType[];
ONC_EXPORT extern const char kIPConfigs[];
ONC_EXPORT extern const char kIPConfigTypeDHCP[];
ONC_EXPORT extern const char kIPConfigTypeStatic[];
ONC_EXPORT extern const char kSavedIPConfig[];
ONC_EXPORT extern const char kStaticIPConfig[];
ONC_EXPORT extern const char kMacAddress[];
ONC_EXPORT extern const char kNameServersConfigType[];
ONC_EXPORT extern const char kName[];
ONC_EXPORT extern const char kPriority[];
ONC_EXPORT extern const char kProxySettings[];
ONC_EXPORT extern const char kSource[];
ONC_EXPORT extern const char kSourceDevice[];
ONC_EXPORT extern const char kSourceDevicePolicy[];
ONC_EXPORT extern const char kSourceNone[];
ONC_EXPORT extern const char kSourceUser[];
ONC_EXPORT extern const char kSourceUserPolicy[];
ONC_EXPORT extern const char kConnectionState[];
ONC_EXPORT extern const char kRestrictedConnectivity[];
ONC_EXPORT extern const char kConnectable[];
ONC_EXPORT extern const char kErrorState[];
ONC_EXPORT extern const char kTether[];
ONC_EXPORT extern const char kType[];
ONC_EXPORT extern const char kVPN[];
ONC_EXPORT extern const char kWiFi[];
ONC_EXPORT extern const char kWimaxDeprecated[];

ONC_EXPORT extern std::string CellularProperty(const std::string& property);
ONC_EXPORT extern std::string TetherProperty(const std::string& property);
ONC_EXPORT extern std::string VpnProperty(const std::string& property);
ONC_EXPORT extern std::string WifiProperty(const std::string& property);

}  // namespace network_config

namespace network_type {
ONC_EXPORT extern const char kCellular[];
ONC_EXPORT extern const char kEthernet[];
ONC_EXPORT extern const char kTether[];
ONC_EXPORT extern const char kVPN[];
ONC_EXPORT extern const char kWiFi[];
ONC_EXPORT extern const char kWimaxDeprecated[];
// Patterns matching multiple types, not part of the ONC spec.
ONC_EXPORT extern const char kAllTypes[];
ONC_EXPORT extern const char kWireless[];
}  // namespace network_type

namespace cellular {
ONC_EXPORT extern const char kActivationState[];
ONC_EXPORT extern const char kActivated[];
ONC_EXPORT extern const char kActivating[];
ONC_EXPORT extern const char kAutoConnect[];
ONC_EXPORT extern const char kNotActivated[];
ONC_EXPORT extern const char kPartiallyActivated[];
ONC_EXPORT extern const char kActivationType[];
ONC_EXPORT extern const char kAllowRoaming[];
ONC_EXPORT extern const char kAPN[];
ONC_EXPORT extern const char kAPNList[];
ONC_EXPORT extern const char kCarrier[];
ONC_EXPORT extern const char kESN[];
ONC_EXPORT extern const char kFamily[];
ONC_EXPORT extern const char kFirmwareRevision[];
ONC_EXPORT extern const char kFoundNetworks[];
ONC_EXPORT extern const char kHardwareRevision[];
ONC_EXPORT extern const char kHomeProvider[];
ONC_EXPORT extern const char kICCID[];
ONC_EXPORT extern const char kIMEI[];
ONC_EXPORT extern const char kIMSI[];
ONC_EXPORT extern const char kLastGoodAPN[];
ONC_EXPORT extern const char kManufacturer[];
ONC_EXPORT extern const char kMDN[];
ONC_EXPORT extern const char kMEID[];
ONC_EXPORT extern const char kMIN[];
ONC_EXPORT extern const char kModelID[];
ONC_EXPORT extern const char kNetworkTechnology[];
ONC_EXPORT extern const char kPaymentPortal[];
ONC_EXPORT extern const char kRoamingHome[];
ONC_EXPORT extern const char kRoamingRequired[];
ONC_EXPORT extern const char kRoamingRoaming[];
ONC_EXPORT extern const char kRoamingState[];
ONC_EXPORT extern const char kScanning[];
ONC_EXPORT extern const char kServingOperator[];
ONC_EXPORT extern const char kSignalStrength[];
ONC_EXPORT extern const char kSIMLockStatus[];
ONC_EXPORT extern const char kSIMPresent[];
ONC_EXPORT extern const char kSupportNetworkScan[];
ONC_EXPORT extern const char kTechnologyCdma1Xrtt[];
ONC_EXPORT extern const char kTechnologyEdge[];
ONC_EXPORT extern const char kTechnologyEvdo[];
ONC_EXPORT extern const char kTechnologyGprs[];
ONC_EXPORT extern const char kTechnologyGsm[];
ONC_EXPORT extern const char kTechnologyHspa[];
ONC_EXPORT extern const char kTechnologyHspaPlus[];
ONC_EXPORT extern const char kTechnologyLte[];
ONC_EXPORT extern const char kTechnologyLteAdvanced[];
ONC_EXPORT extern const char kTechnologyUmts[];
}  // namespace cellular

namespace cellular_provider {
ONC_EXPORT extern const char kCode[];
ONC_EXPORT extern const char kCountry[];
ONC_EXPORT extern const char kName[];
}  // namespace cellular_provider

namespace cellular_apn {
ONC_EXPORT extern const char kAccessPointName[];
ONC_EXPORT extern const char kName[];
ONC_EXPORT extern const char kUsername[];
ONC_EXPORT extern const char kPassword[];
ONC_EXPORT extern const char kAuthentication[];
ONC_EXPORT extern const char kLocalizedName[];
ONC_EXPORT extern const char kLanguage[];
}  // namespace cellular_apn

namespace cellular_found_network {
ONC_EXPORT extern const char kStatus[];
ONC_EXPORT extern const char kNetworkId[];
ONC_EXPORT extern const char kShortName[];
ONC_EXPORT extern const char kLongName[];
ONC_EXPORT extern const char kTechnology[];
}  // namespace cellular_found_network

namespace cellular_payment_portal {
ONC_EXPORT extern const char kMethod[];
ONC_EXPORT extern const char kPostData[];
ONC_EXPORT extern const char kUrl[];
}  // namespace cellular_payment_portal

namespace sim_lock_status {
ONC_EXPORT extern const char kLockEnabled[];
ONC_EXPORT extern const char kLockType[];
ONC_EXPORT extern const char kRetriesLeft[];
}  // namespace sim_lock_status

namespace connection_state {
ONC_EXPORT extern const char kConnected[];
ONC_EXPORT extern const char kConnecting[];
ONC_EXPORT extern const char kNotConnected[];
}  // namespace connection_state

namespace ipconfig {
ONC_EXPORT extern const char kGateway[];
ONC_EXPORT extern const char kIPAddress[];
ONC_EXPORT extern const char kIPv4[];
ONC_EXPORT extern const char kIPv6[];
ONC_EXPORT extern const char kNameServers[];
ONC_EXPORT extern const char kRoutingPrefix[];
ONC_EXPORT extern const char kSearchDomains[];
ONC_EXPORT extern const char kIncludedRoutes[];
ONC_EXPORT extern const char kExcludedRoutes[];
ONC_EXPORT extern const char kType[];
ONC_EXPORT extern const char kWebProxyAutoDiscoveryUrl[];
}  // namespace ipconfig

namespace ethernet {
ONC_EXPORT extern const char kAuthentication[];
ONC_EXPORT extern const char kAuthenticationNone[];
ONC_EXPORT extern const char kEAP[];
ONC_EXPORT extern const char k8021X[];
}  // namespace ethernet

namespace tether {
ONC_EXPORT extern const char kBatteryPercentage[];
ONC_EXPORT extern const char kCarrier[];
ONC_EXPORT extern const char kHasConnectedToHost[];
ONC_EXPORT extern const char kSignalStrength[];
}  // namespace tether

namespace wifi {
ONC_EXPORT extern const char kAllowGatewayARPPolling[];
ONC_EXPORT extern const char kAutoConnect[];
ONC_EXPORT extern const char kBSSID[];
ONC_EXPORT extern const char kEAP[];
ONC_EXPORT extern const char kFrequency[];
ONC_EXPORT extern const char kFrequencyList[];
ONC_EXPORT extern const char kFTEnabled[];
ONC_EXPORT extern const char kHexSSID[];
ONC_EXPORT extern const char kHiddenSSID[];
ONC_EXPORT extern const char kPassphrase[];
ONC_EXPORT extern const char kRoamThreshold[];
ONC_EXPORT extern const char kSSID[];
ONC_EXPORT extern const char kSecurity[];
ONC_EXPORT extern const char kSecurityNone[];
ONC_EXPORT extern const char kSignalStrength[];
ONC_EXPORT extern const char kTetheringState[];
ONC_EXPORT extern const char kWEP_PSK[];
ONC_EXPORT extern const char kWEP_8021X[];
ONC_EXPORT extern const char kWPA_PSK[];
ONC_EXPORT extern const char kWPA2_PSK[];
ONC_EXPORT extern const char kWPA_EAP[];
}  // namespace wifi

namespace wimax_deprecated {
ONC_EXPORT extern const char kAutoConnect[];
ONC_EXPORT extern const char kEAP[];
}  // namespace wimax_deprecated

namespace client_cert {
ONC_EXPORT extern const char kClientCertPattern[];
ONC_EXPORT extern const char kClientCertPKCS11Id[];
ONC_EXPORT extern const char kClientCertRef[];
ONC_EXPORT extern const char kClientCertType[];
ONC_EXPORT extern const char kClientCertTypeNone[];
ONC_EXPORT extern const char kCommonName[];
ONC_EXPORT extern const char kEmailAddress[];
ONC_EXPORT extern const char kEnrollmentURI[];
ONC_EXPORT extern const char kIssuerCARef[];
ONC_EXPORT extern const char kIssuerCAPEMs[];
ONC_EXPORT extern const char kIssuer[];
ONC_EXPORT extern const char kLocality[];
ONC_EXPORT extern const char kOrganization[];
ONC_EXPORT extern const char kOrganizationalUnit[];
ONC_EXPORT extern const char kPattern[];
ONC_EXPORT extern const char kPKCS11Id[];
ONC_EXPORT extern const char kRef[];
ONC_EXPORT extern const char kSubject[];
}  // namespace client_cert

namespace certificate {
ONC_EXPORT extern const char kAuthority[];
ONC_EXPORT extern const char kClient[];
ONC_EXPORT extern const char kGUID[];
ONC_EXPORT extern const char kPKCS12[];
ONC_EXPORT extern const char kScope[];
ONC_EXPORT extern const char kServer[];
ONC_EXPORT extern const char kTrustBits[];
ONC_EXPORT extern const char kType[];
ONC_EXPORT extern const char kWeb[];
ONC_EXPORT extern const char kX509[];
}  // namespace certificate

namespace scope {
ONC_EXPORT extern const char kDefault[];
ONC_EXPORT extern const char kExtension[];
ONC_EXPORT extern const char kId[];
ONC_EXPORT extern const char kType[];
}  // namespace scope

namespace encrypted {
ONC_EXPORT extern const char kAES256[];
ONC_EXPORT extern const char kCipher[];
ONC_EXPORT extern const char kCiphertext[];
ONC_EXPORT extern const char kHMACMethod[];
ONC_EXPORT extern const char kHMAC[];
ONC_EXPORT extern const char kIV[];
ONC_EXPORT extern const char kIterations[];
ONC_EXPORT extern const char kPBKDF2[];
ONC_EXPORT extern const char kSHA1[];
ONC_EXPORT extern const char kSalt[];
ONC_EXPORT extern const char kStretch[];
}  // namespace encrypted

namespace eap {
ONC_EXPORT extern const char kAnonymousIdentity[];
ONC_EXPORT extern const char kAutomatic[];
ONC_EXPORT extern const char kEAP_AKA[];
ONC_EXPORT extern const char kEAP_FAST[];
ONC_EXPORT extern const char kEAP_SIM[];
ONC_EXPORT extern const char kEAP_TLS[];
ONC_EXPORT extern const char kEAP_TTLS[];
ONC_EXPORT extern const char kGTC[];
ONC_EXPORT extern const char kIdentity[];
ONC_EXPORT extern const char kInner[];
ONC_EXPORT extern const char kLEAP[];
ONC_EXPORT extern const char kMD5[];
ONC_EXPORT extern const char kMSCHAP[];
ONC_EXPORT extern const char kMSCHAPv2[];
ONC_EXPORT extern const char kOuter[];
ONC_EXPORT extern const char kPAP[];
ONC_EXPORT extern const char kPEAP[];
ONC_EXPORT extern const char kPassword[];
ONC_EXPORT extern const char kSaveCredentials[];
ONC_EXPORT extern const char kServerCAPEMs[];
ONC_EXPORT extern const char kServerCARef[];
ONC_EXPORT extern const char kServerCARefs[];
ONC_EXPORT extern const char kSubjectMatch[];
ONC_EXPORT extern const char kTLSVersionMax[];
ONC_EXPORT extern const char kUseSystemCAs[];
ONC_EXPORT extern const char kUseProactiveKeyCaching[];
}  // namespace eap

namespace vpn {
ONC_EXPORT extern const char kAutoConnect[];
ONC_EXPORT extern const char kHost[];
ONC_EXPORT extern const char kIPsec[];
ONC_EXPORT extern const char kL2TP[];
ONC_EXPORT extern const char kOpenVPN[];
ONC_EXPORT extern const char kPassword[];
ONC_EXPORT extern const char kSaveCredentials[];
ONC_EXPORT extern const char kThirdPartyVpn[];
ONC_EXPORT extern const char kArcVpn[];
ONC_EXPORT extern const char kTypeL2TP_IPsec[];
ONC_EXPORT extern const char kType[];
ONC_EXPORT extern const char kUsername[];
}  // namespace vpn

namespace ipsec {
ONC_EXPORT extern const char kAuthenticationType[];
ONC_EXPORT extern const char kCert[];
ONC_EXPORT extern const char kEAP[];
ONC_EXPORT extern const char kGroup[];
ONC_EXPORT extern const char kIKEVersion[];
ONC_EXPORT extern const char kPSK[];
ONC_EXPORT extern const char kServerCAPEMs[];
ONC_EXPORT extern const char kServerCARef[];
ONC_EXPORT extern const char kServerCARefs[];
ONC_EXPORT extern const char kXAUTH[];
}  // namespace ipsec

namespace l2tp {
ONC_EXPORT extern const char kLcpEchoDisabled[];
ONC_EXPORT extern const char kPassword[];
ONC_EXPORT extern const char kSaveCredentials[];
ONC_EXPORT extern const char kUsername[];
}  // namespace l2tp

namespace openvpn {
ONC_EXPORT extern const char kAuthNoCache[];
ONC_EXPORT extern const char kAuthRetry[];
ONC_EXPORT extern const char kAuth[];
ONC_EXPORT extern const char kCipher[];
ONC_EXPORT extern const char kCompLZO[];
ONC_EXPORT extern const char kCompNoAdapt[];
ONC_EXPORT extern const char kExtraHosts[];
ONC_EXPORT extern const char kIgnoreDefaultRoute[];
ONC_EXPORT extern const char kInteract[];
ONC_EXPORT extern const char kKeyDirection[];
ONC_EXPORT extern const char kNoInteract[];
ONC_EXPORT extern const char kNone[];
ONC_EXPORT extern const char kNsCertType[];
ONC_EXPORT extern const char kOTP[];
ONC_EXPORT extern const char kPassword[];
ONC_EXPORT extern const char kPort[];
ONC_EXPORT extern const char kProto[];
ONC_EXPORT extern const char kPushPeerInfo[];
ONC_EXPORT extern const char kRemoteCertEKU[];
ONC_EXPORT extern const char kRemoteCertKU[];
ONC_EXPORT extern const char kRemoteCertTLS[];
ONC_EXPORT extern const char kRenegSec[];
ONC_EXPORT extern const char kServerCAPEMs[];
ONC_EXPORT extern const char kServerCARef[];
ONC_EXPORT extern const char kServerCARefs[];
ONC_EXPORT extern const char kServerCertPEM[];
ONC_EXPORT extern const char kServerCertRef[];
ONC_EXPORT extern const char kServerPollTimeout[];
ONC_EXPORT extern const char kServer[];
ONC_EXPORT extern const char kShaper[];
ONC_EXPORT extern const char kStaticChallenge[];
ONC_EXPORT extern const char kTLSAuthContents[];
ONC_EXPORT extern const char kTLSRemote[];
ONC_EXPORT extern const char kTLSVersionMin[];
ONC_EXPORT extern const char kUserAuthenticationType[];
ONC_EXPORT extern const char kVerb[];
ONC_EXPORT extern const char kVerifyHash[];
ONC_EXPORT extern const char kVerifyX509[];
}  // namespace openvpn

namespace openvpn_user_auth_type {
ONC_EXPORT extern const char kNone[];
ONC_EXPORT extern const char kOTP[];
ONC_EXPORT extern const char kPassword[];
ONC_EXPORT extern const char kPasswordAndOTP[];
}  // openvpn_user_auth_type

namespace third_party_vpn {
ONC_EXPORT extern const char kExtensionID[];
ONC_EXPORT extern const char kProviderName[];
}  // third_party_vpn

namespace arc_vpn {
ONC_EXPORT extern const char kTunnelChrome[];
}  // namespace arc_vpn

namespace verify_x509 {
ONC_EXPORT extern const char kName[];
ONC_EXPORT extern const char kType[];

namespace types {
ONC_EXPORT extern const char kName[];
ONC_EXPORT extern const char kNamePrefix[];
ONC_EXPORT extern const char kSubject[];
}  // namespace types
}  // namespace verify_x509

namespace substitutes {
ONC_EXPORT extern const char kLoginEmail[];
ONC_EXPORT extern const char kLoginID[];
ONC_EXPORT extern const char kCertSANEmail[];
ONC_EXPORT extern const char kCertSANUPN[];
ONC_EXPORT extern const char kCertSubjectCommonName[];
ONC_EXPORT extern const char kDeviceSerialNumber[];
ONC_EXPORT extern const char kDeviceAssetId[];
ONC_EXPORT extern const char kPasswordPlaceholderVerbatim[];
}  // namespace substitutes

namespace proxy {
ONC_EXPORT extern const char kDirect[];
ONC_EXPORT extern const char kExcludeDomains[];
ONC_EXPORT extern const char kFtp[];
ONC_EXPORT extern const char kHost[];
ONC_EXPORT extern const char kHttp[];
ONC_EXPORT extern const char kHttps[];
ONC_EXPORT extern const char kManual[];
ONC_EXPORT extern const char kPAC[];
ONC_EXPORT extern const char kPort[];
ONC_EXPORT extern const char kSocks[];
ONC_EXPORT extern const char kType[];
ONC_EXPORT extern const char kWPAD[];
}  // namespace proxy

namespace global_network_config {
ONC_EXPORT extern const char kAllowOnlyPolicyNetworksToAutoconnect[];
ONC_EXPORT extern const char kAllowOnlyPolicyNetworksToConnect[];
ONC_EXPORT extern const char kAllowOnlyPolicyNetworksToConnectIfAvailable[];
ONC_EXPORT extern const char kBlacklistedHexSSIDs[];
ONC_EXPORT extern const char kDisableNetworkTypes[];
}  // global_network_config

namespace device_state {
ONC_EXPORT extern const char kUninitialized[];
ONC_EXPORT extern const char kDisabled[];
ONC_EXPORT extern const char kEnabling[];
ONC_EXPORT extern const char kEnabled[];
}  // device_state

namespace tethering_state {
ONC_EXPORT extern const char kTetheringConfirmedState[];
ONC_EXPORT extern const char kTetheringNotDetectedState[];
ONC_EXPORT extern const char kTetheringSuspectedState[];
}  // namespace tethering_state

}  // namespace onc

#endif  // COMPONENTS_ONC_ONC_CONSTANTS_H_
