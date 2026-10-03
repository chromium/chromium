// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ONC_ONC_CONSTANTS_H_
#define COMPONENTS_ONC_ONC_CONSTANTS_H_

#include <string>
#include <string_view>

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
inline constexpr char kAugmentationActiveSetting[] = "Active";
// The one of different setting sources (user/device policy, user/shared
// settings) that has highest priority over the others.
inline constexpr char kAugmentationEffectiveSetting[] = "Effective";
inline constexpr char kAugmentationUserPolicy[] = "UserPolicy";
inline constexpr char kAugmentationDevicePolicy[] = "DevicePolicy";
inline constexpr char kAugmentationUserSetting[] = "UserSetting";
inline constexpr char kAugmentationSharedSetting[] = "SharedSetting";
inline constexpr char kAugmentationUserEditable[] = "UserEditable";
inline constexpr char kAugmentationDeviceEditable[] = "DeviceEditable";

// Special key for indicating that the Effective value is the Active value
// and was set by an extension. Used for ProxySettings.
inline constexpr char kAugmentationActiveExtension[] = "ActiveExtension";

// Common keys/values.
inline constexpr char kRecommended[] = "Recommended";
inline constexpr char kRemove[] = "Remove";

// Top Level Configuration
namespace toplevel_config {
inline constexpr char kAdminAPNList[] = "AdminAPNList";
inline constexpr char kCertificates[] = "Certificates";
inline constexpr char kEncryptedConfiguration[] = "EncryptedConfiguration";
inline constexpr char kNetworkConfigurations[] = "NetworkConfigurations";
inline constexpr char kGlobalNetworkConfiguration[] =
    "GlobalNetworkConfiguration";
inline constexpr char kType[] = "Type";
inline constexpr char kUnencryptedConfiguration[] = "UnencryptedConfiguration";
}  // namespace toplevel_config

// NetworkConfiguration.
namespace network_config {
inline constexpr char kCellular[] = "Cellular";
inline constexpr char kCheckCaptivePortal[] = "CheckCaptivePortal";
inline constexpr char kDevice[] = "Device";
inline constexpr char kEthernet[] = "Ethernet";
inline constexpr char kGUID[] = "GUID";
inline constexpr char kIPAddressConfigType[] = "IPAddressConfigType";
inline constexpr char kIPConfigs[] = "IPConfigs";
inline constexpr char kIPConfigTypeDHCP[] = "DHCP";
inline constexpr char kIPConfigTypeStatic[] = "Static";
inline constexpr char kSavedIPConfig[] = "SavedIPConfig";
inline constexpr char kStaticIPConfig[] = "StaticIPConfig";
inline constexpr char kMacAddress[] = "MacAddress";
inline constexpr char kMetered[] = "Metered";
inline constexpr char kNameServersConfigType[] = "NameServersConfigType";
inline constexpr char kName[] = "Name";
inline constexpr char kPriority[] = "Priority";
inline constexpr char kProxySettings[] = "ProxySettings";
inline constexpr char kSource[] = "Source";
inline constexpr char kSourceDevice[] = "Device";
inline constexpr char kSourceDevicePolicy[] = "DevicePolicy";
inline constexpr char kSourceNone[] = "None";
inline constexpr char kSourceUser[] = "User";
inline constexpr char kSourceUserPolicy[] = "UserPolicy";
inline constexpr char kConnectionState[] = "ConnectionState";
inline constexpr char kRestrictedConnectivity[] = "RestrictedConnectivity";
inline constexpr char kConnectable[] = "Connectable";
inline constexpr char kErrorState[] = "ErrorState";
inline constexpr char kTether[] = "Tether";
inline constexpr char kTrafficCounterResetTime[] = "TrafficCounterResetTime";
inline constexpr char kType[] = "Type";
inline constexpr char kVPN[] = "VPN";
inline constexpr char kWiFi[] = "WiFi";
inline constexpr char kWimaxDeprecated[] = "WiMAX";

COMPONENT_EXPORT(ONC)
extern std::string CellularProperty(std::string_view property);
COMPONENT_EXPORT(ONC)
extern std::string TetherProperty(std::string_view property);
COMPONENT_EXPORT(ONC)
extern std::string VpnProperty(std::string_view property);
COMPONENT_EXPORT(ONC)
extern std::string WifiProperty(std::string_view property);

}  // namespace network_config

namespace network_type {
inline constexpr char kCellular[] = "Cellular";
inline constexpr char kEthernet[] = "Ethernet";
inline constexpr char kTether[] = "Tether";
inline constexpr char kVPN[] = "VPN";
inline constexpr char kWiFi[] = "WiFi";
inline constexpr char kWimaxDeprecated[] = "WiMAX";
// Patterns matching multiple types, not part of the ONC spec.
inline constexpr char kAllTypes[] = "All";
inline constexpr char kWireless[] = "Wireless";
}  // namespace network_type

namespace check_captive_portal {
inline constexpr char kFalse[] = "False";
inline constexpr char kHTTPOnly[] = "HTTPOnly";
inline constexpr char kTrue[] = "True";
}  // namespace check_captive_portal

namespace cellular {
inline constexpr char kActivationState[] = "ActivationState";
inline constexpr char kActivated[] = "Activated";
inline constexpr char kActivating[] = "Activating";
inline constexpr char kAutoConnect[] = "AutoConnect";
inline constexpr char kNotActivated[] = "NotActivated";
inline constexpr char kPartiallyActivated[] = "PartiallyActivated";
inline constexpr char kActivationType[] = "ActivationType";
inline constexpr char kAdminAssignedAPNIds[] = "AdminAssignedAPNIds";
inline constexpr char kAllowRoaming[] = "AllowRoaming";
inline constexpr char kAPN[] = "APN";
inline constexpr char kAPNList[] = "APNList";
inline constexpr char kCustomAPNList[] = "CustomAPNList";
inline constexpr char kESN[] = "ESN";
inline constexpr char kFamily[] = "Family";
inline constexpr char kFirmwareRevision[] = "FirmwareRevision";
inline constexpr char kFoundNetworks[] = "FoundNetworks";
inline constexpr char kHardwareRevision[] = "HardwareRevision";
inline constexpr char kHomeProvider[] = "HomeProvider";
inline constexpr char kEID[] = "EID";
inline constexpr char kICCID[] = "ICCID";
inline constexpr char kIMEI[] = "IMEI";
inline constexpr char kIMSI[] = "IMSI";
inline constexpr char kLastConnectedAttachApnProperty[] =
    "LastConnectedAttachApnProperty";
inline constexpr char kLastConnectedDefaultApnProperty[] =
    "LastConnectedDefaultApnProperty";
inline constexpr char kLastGoodAPN[] = "LastGoodAPN";
inline constexpr char kManufacturer[] = "Manufacturer";
inline constexpr char kMDN[] = "MDN";
inline constexpr char kMEID[] = "MEID";
inline constexpr char kMIN[] = "MIN";
inline constexpr char kModelID[] = "ModelID";
inline constexpr char kNetworkTechnology[] = "NetworkTechnology";
inline constexpr char kPaymentPortal[] = "PaymentPortal";
inline constexpr char kRoamingHome[] = "Home";
inline constexpr char kRoamingRequired[] = "Required";
inline constexpr char kRoamingRoaming[] = "Roaming";
inline constexpr char kRoamingState[] = "RoamingState";
inline constexpr char kScanning[] = "Scanning";
inline constexpr char kServingOperator[] = "ServingOperator";
inline constexpr char kSignalStrength[] = "SignalStrength";
inline constexpr char kSIMLockStatus[] = "SIMLockStatus";
inline constexpr char kSIMPresent[] = "SIMPresent";
inline constexpr char kSMDPAddress[] = "SMDPAddress";
inline constexpr char kSMDSAddress[] = "SMDSAddress";
inline constexpr char kSupportNetworkScan[] = "SupportNetworkScan";
inline constexpr char kTechnologyCdma1Xrtt[] = "CDMA1XRTT";
inline constexpr char kTechnologyEdge[] = "EDGE";
inline constexpr char kTechnologyEvdo[] = "EVDO";
inline constexpr char kTechnologyGprs[] = "GPRS";
inline constexpr char kTechnologyGsm[] = "GSM";
inline constexpr char kTechnologyHspa[] = "HSPA";
inline constexpr char kTechnologyHspaPlus[] = "HSPAPlus";
inline constexpr char kTechnologyLte[] = "LTE";
inline constexpr char kTechnologyLteAdvanced[] = "LTEAdvanced";
inline constexpr char kTechnologyUmts[] = "UMTS";
inline constexpr char kTechnology5gNr[] = "5GNR";
inline constexpr char kTextMessagesAllow[] = "Allow";
inline constexpr char kTextMessagesSuppress[] = "Suppress";
inline constexpr char kTextMessagesUnset[] = "Unset";
}  // namespace cellular

namespace cellular_provider {
inline constexpr char kCode[] = "Code";
inline constexpr char kCountry[] = "Country";
inline constexpr char kName[] = "Name";
}  // namespace cellular_provider

namespace cellular_apn {
inline constexpr char kAccessPointName[] = "AccessPointName";
inline constexpr char kName[] = "Name";
inline constexpr char kUsername[] = "Username";
inline constexpr char kPassword[] = "Password";
inline constexpr char kAuthentication[] = "Authentication";
inline constexpr char kLocalizedName[] = "LocalizedName";
inline constexpr char kLanguage[] = "Language";
inline constexpr char kAttach[] = "Attach";
inline constexpr char kId[] = "Id";
inline constexpr char kState[] = "State";
inline constexpr char kStateEnabled[] = "Enabled";
inline constexpr char kStateDisabled[] = "Disabled";
inline constexpr char kAuthenticationAutomatic[] = "";
inline constexpr char kAuthenticationPap[] = "PAP";
inline constexpr char kAuthenticationChap[] = "CHAP";
inline constexpr char kIpType[] = "IpType";
inline constexpr char kIpTypeAutomatic[] = "";
inline constexpr char kIpTypeIpv4[] = "IPv4";
inline constexpr char kIpTypeIpv6[] = "IPv6";
inline constexpr char kIpTypeIpv4Ipv6[] = "IPv4orIPv6";
inline constexpr char kApnTypes[] = "ApnTypes";
inline constexpr char kApnTypeDefault[] = "Default";
inline constexpr char kApnTypeAttach[] = "Attach";
inline constexpr char kApnTypeTether[] = "Tether";
inline constexpr char kSource[] = "Source";
inline constexpr char kSourceUi[] = "Ui";
inline constexpr char kSourceAdmin[] = "Admin";
inline constexpr char kSourceModb[] = "Modb";
inline constexpr char kSourceModem[] = "Modem";
}  // namespace cellular_apn

namespace cellular_found_network {
inline constexpr char kStatus[] = "Status";
inline constexpr char kNetworkId[] = "NetworkId";
inline constexpr char kShortName[] = "ShortName";
inline constexpr char kLongName[] = "LongName";
inline constexpr char kTechnology[] = "Technology";
}  // namespace cellular_found_network

namespace cellular_payment_portal {
inline constexpr char kMethod[] = "Method";
inline constexpr char kPostData[] = "PostData";
inline constexpr char kUrl[] = "Url";
}  // namespace cellular_payment_portal

namespace sim_lock_status {
inline constexpr char kLockEnabled[] = "LockEnabled";
inline constexpr char kLockType[] = "LockType";
inline constexpr char kRetriesLeft[] = "RetriesLeft";
}  // namespace sim_lock_status

namespace connection_state {
inline constexpr char kConnected[] = "Connected";
inline constexpr char kConnecting[] = "Connecting";
inline constexpr char kNotConnected[] = "NotConnected";
}  // namespace connection_state

namespace ipconfig {
inline constexpr char kGateway[] = "Gateway";
inline constexpr char kIPAddress[] = "IPAddress";
inline constexpr char kIPv4[] = "IPv4";
inline constexpr char kIPv6[] = "IPv6";
inline constexpr char kNameServers[] = "NameServers";
inline constexpr char kRoutingPrefix[] = "RoutingPrefix";
inline constexpr char kSearchDomains[] = "SearchDomains";
inline constexpr char kIncludedRoutes[] = "IncludedRoutes";
inline constexpr char kExcludedRoutes[] = "ExcludedRoutes";
inline constexpr char kType[] = "Type";
inline constexpr char kWebProxyAutoDiscoveryUrl[] = "WebProxyAutoDiscoveryUrl";
inline constexpr char kMTU[] = "MTU";
}  // namespace ipconfig

namespace ethernet {
inline constexpr char kAuthentication[] = "Authentication";
inline constexpr char kAuthenticationNone[] = "None";
inline constexpr char kEAP[] = "EAP";
inline constexpr char k8021X[] = "8021X";
}  // namespace ethernet

namespace tether {
inline constexpr char kBatteryPercentage[] = "BatteryPercentage";
inline constexpr char kCarrier[] = "Carrier";
inline constexpr char kHasConnectedToHost[] = "HasConnectedToHost";
inline constexpr char kSignalStrength[] = "SignalStrength";
}  // namespace tether

namespace wifi {
inline constexpr char kAllowGatewayARPPolling[] = "AllowGatewayARPPolling";
inline constexpr char kAutoConnect[] = "AutoConnect";
inline constexpr char kBSSID[] = "BSSID";
inline constexpr char kBSSIDAllowlist[] = "BSSIDAllowlist";
inline constexpr char kBSSIDRequested[] = "BSSIDRequested";
inline constexpr char kEAP[] = "EAP";
inline constexpr char kFrequency[] = "Frequency";
inline constexpr char kFrequencyList[] = "FrequencyList";
inline constexpr char kHexSSID[] = "HexSSID";
inline constexpr char kHiddenSSID[] = "HiddenSSID";
inline constexpr char kPassphrase[] = "Passphrase";
inline constexpr char kSSID[] = "SSID";
inline constexpr char kSecurity[] = "Security";
inline constexpr char kSecurityNone[] = "None";
inline constexpr char kSignalStrength[] = "SignalStrength";
inline constexpr char kSignalStrengthRssi[] = "SignalStrengthRssi";
inline constexpr char kWEP_PSK[] = "WEP-PSK";
inline constexpr char kWEP_8021X[] = "WEP-8021X";
inline constexpr char kWPA_PSK[] = "WPA-PSK";
inline constexpr char kWPA2_PSK[] = "WPA2-PSK";
inline constexpr char kWPA_EAP[] = "WPA-EAP";
inline constexpr char kPasspointId[] = "PasspointId";
inline constexpr char kPasspointMatchType[] = "PasspointMatchType";
}  // namespace wifi

// Deprecated, properties exist for ignoring old ONC config entries.
namespace wimax_deprecated {
inline constexpr char kAutoConnect[] = "AutoConnect";
inline constexpr char kEAP[] = "EAP";
}  // namespace wimax_deprecated

namespace client_cert {
inline constexpr char kClientCertProvisioningProfileId[] =
    "ClientCertProvisioningProfileId";
inline constexpr char kClientCertPattern[] = "ClientCertPattern";
inline constexpr char kClientCertPKCS11Id[] = "ClientCertPKCS11Id";
inline constexpr char kClientCertRef[] = "ClientCertRef";
inline constexpr char kClientCertType[] = "ClientCertType";
inline constexpr char kClientCertTypeNone[] = "None";
inline constexpr char kCommonName[] = "CommonName";
inline constexpr char kEmailAddress[] = "EmailAddress";
inline constexpr char kEnrollmentURI[] = "EnrollmentURI";
inline constexpr char kIssuerCARef[] = "IssuerCARef";
inline constexpr char kIssuerCAPEMs[] = "IssuerCAPEMs";
inline constexpr char kIssuer[] = "Issuer";
inline constexpr char kLocality[] = "Locality";
inline constexpr char kOrganization[] = "Organization";
inline constexpr char kOrganizationalUnit[] = "OrganizationalUnit";
inline constexpr char kPattern[] = "Pattern";
inline constexpr char kProvisioningProfileId[] = "ProvisioningProfileId";
inline constexpr char kPKCS11Id[] = "PKCS11Id";
inline constexpr char kRef[] = "Ref";
inline constexpr char kSubject[] = "Subject";
}  // namespace client_cert

namespace certificate {
inline constexpr char kAuthority[] = "Authority";
inline constexpr char kClient[] = "Client";
inline constexpr char kGUID[] = "GUID";
inline constexpr char kPKCS12[] = "PKCS12";
inline constexpr char kScope[] = "Scope";
inline constexpr char kServer[] = "Server";
inline constexpr char kTrustBits[] = "TrustBits";
inline constexpr char kType[] = "Type";
inline constexpr char kWeb[] = "Web";
inline constexpr char kX509[] = "X509";
}  // namespace certificate

namespace scope {
inline constexpr char kDefault[] = "Default";
inline constexpr char kExtension[] = "Extension";
inline constexpr char kId[] = "Id";
inline constexpr char kType[] = "Type";
}  // namespace scope

namespace encrypted {
inline constexpr char kAES256[] = "AES256";
inline constexpr char kCipher[] = "Cipher";
inline constexpr char kCiphertext[] = "Ciphertext";
inline constexpr char kHMACMethod[] = "HMACMethod";
inline constexpr char kHMAC[] = "HMAC";
inline constexpr char kIV[] = "IV";
inline constexpr char kIterations[] = "Iterations";
inline constexpr char kPBKDF2[] = "PBKDF2";
inline constexpr char kSHA1[] = "SHA1";
inline constexpr char kSalt[] = "Salt";
inline constexpr char kStretch[] = "Stretch";
}  // namespace encrypted

namespace eap {
inline constexpr char kAnonymousIdentity[] = "AnonymousIdentity";
inline constexpr char kAutomatic[] = "Automatic";
inline constexpr char kCHAP[] = "CHAP";
inline constexpr char kDomainSuffixMatch[] = "DomainSuffixMatch";
inline constexpr char kEAP_AKA[] = "EAP-AKA";
inline constexpr char kEAP_FAST[] = "EAP-FAST";
inline constexpr char kEAP_SIM[] = "EAP-SIM";
inline constexpr char kEAP_TLS[] = "EAP-TLS";
inline constexpr char kEAP_TTLS[] = "EAP-TTLS";
inline constexpr char kGTC[] = "GTC";
inline constexpr char kIdentity[] = "Identity";
inline constexpr char kInner[] = "Inner";
inline constexpr char kLEAP[] = "LEAP";
inline constexpr char kMD5[] = "MD5";
inline constexpr char kMSCHAP[] = "MSCHAP";
inline constexpr char kMSCHAPv2[] = "MSCHAPv2";
inline constexpr char kOuter[] = "Outer";
inline constexpr char kPAP[] = "PAP";
inline constexpr char kPEAP[] = "PEAP";
inline constexpr char kPassword[] = "Password";
inline constexpr char kSaveCredentials[] = "SaveCredentials";
inline constexpr char kServerCAPEMs[] = "ServerCAPEMs";
inline constexpr char kServerCARef[] = "ServerCARef";
inline constexpr char kServerCARefs[] = "ServerCARefs";
inline constexpr char kSubjectMatch[] = "SubjectMatch";
inline constexpr char kSubjectAlternativeNameMatch[] =
    "SubjectAlternativeNameMatch";
inline constexpr char kTLSVersionMax[] = "TLSVersionMax";
inline constexpr char kUseSystemCAs[] = "UseSystemCAs";
inline constexpr char kUseProactiveKeyCaching[] = "UseProactiveKeyCaching";
}  // namespace eap

namespace eap_subject_alternative_name_match {
inline constexpr char kType[] = "Type";
inline constexpr char kValue[] = "Value";
inline constexpr char kEMAIL[] = "EMAIL";
inline constexpr char kDNS[] = "DNS";
inline constexpr char kURI[] = "URI";
}  // namespace eap_subject_alternative_name_match

namespace vpn {
inline constexpr char kArcVpn[] = "ARCVPN";
inline constexpr char kAutoConnect[] = "AutoConnect";
inline constexpr char kHost[] = "Host";
inline constexpr char kIPsec[] = "IPsec";
inline constexpr char kL2TP[] = "L2TP";
inline constexpr char kOpenVPN[] = "OpenVPN";
inline constexpr char kPassword[] = "Password";
inline constexpr char kSaveCredentials[] = "SaveCredentials";
inline constexpr char kThirdPartyVpn[] = "ThirdPartyVPN";
inline constexpr char kTypeL2TP_IPsec[] = "L2TP-IPsec";
inline constexpr char kType[] = "Type";
inline constexpr char kUsername[] = "Username";
inline constexpr char kWireGuard[] = "WireGuard";
}  // namespace vpn

namespace ipsec {
inline constexpr char kAuthenticationType[] = "AuthenticationType";
inline constexpr char kCert[] = "Cert";
inline constexpr char kEAP[] = "EAP";
inline constexpr char kGroup[] = "Group";
inline constexpr char kIKEVersion[] = "IKEVersion";
inline constexpr char kLocalIdentity[] = "LocalIdentity";
inline constexpr char kPSK[] = "PSK";
inline constexpr char kRemoteIdentity[] = "RemoteIdentity";
inline constexpr char kServerCAPEMs[] = "ServerCAPEMs";
inline constexpr char kServerCARef[] = "ServerCARef";
inline constexpr char kServerCARefs[] = "ServerCARefs";
inline constexpr char kXAUTH[] = "XAUTH";
}  // namespace ipsec

namespace l2tp {
inline constexpr char kLcpEchoDisabled[] = "LcpEchoDisabled";
inline constexpr char kPassword[] = "Password";
inline constexpr char kSaveCredentials[] = "SaveCredentials";
inline constexpr char kUsername[] = "Username";
}  // namespace l2tp

namespace openvpn {
inline constexpr char kAuthNoCache[] = "AuthNoCache";
inline constexpr char kAuthRetry[] = "AuthRetry";
inline constexpr char kAuth[] = "Auth";
inline constexpr char kCipher[] = "Cipher";
inline constexpr char kCompLZO[] = "CompLZO";
inline constexpr char kCompNoAdapt[] = "CompNoAdapt";
inline constexpr char kCompressionAlgorithm[] = "CompressionAlgorithm";
inline constexpr char kExtraHosts[] = "ExtraHosts";
inline constexpr char kIgnoreDefaultRoute[] = "IgnoreDefaultRoute";
inline constexpr char kInteract[] = "interact";
inline constexpr char kKeyDirection[] = "KeyDirection";
inline constexpr char kNoInteract[] = "nointeract";
inline constexpr char kNone[] = "none";
inline constexpr char kNsCertType[] = "NsCertType";
inline constexpr char kOTP[] = "OTP";
inline constexpr char kPassword[] = "Password";
inline constexpr char kPort[] = "Port";
inline constexpr char kProto[] = "Proto";
inline constexpr char kPushPeerInfo[] = "PushPeerInfo";
inline constexpr char kRemoteCertEKU[] = "RemoteCertEKU";
inline constexpr char kRemoteCertKU[] = "RemoteCertKU";
inline constexpr char kRemoteCertTLS[] = "RemoteCertTLS";
inline constexpr char kRenegSec[] = "RenegSec";
inline constexpr char kServerCAPEMs[] = "ServerCAPEMs";
inline constexpr char kServerCARef[] = "ServerCARef";
inline constexpr char kServerCARefs[] = "ServerCARefs";
inline constexpr char kServerCertPEM[] = "ServerCertPEM";
inline constexpr char kServerCertRef[] = "ServerCertRef";
inline constexpr char kServerPollTimeout[] = "ServerPollTimeout";
inline constexpr char kServer[] = "server";
inline constexpr char kShaper[] = "Shaper";
inline constexpr char kStaticChallenge[] = "StaticChallenge";
inline constexpr char kTLSAuthContents[] = "TLSAuthContents";
inline constexpr char kTLSRemote[] = "TLSRemote";
inline constexpr char kTLSVersionMin[] = "TLSVersionMin";
inline constexpr char kUserAuthenticationType[] = "UserAuthenticationType";
inline constexpr char kVerb[] = "Verb";
inline constexpr char kVerifyHash[] = "VerifyHash";
inline constexpr char kVerifyX509[] = "VerifyX509";
}  // namespace openvpn

namespace wireguard {
inline constexpr char kAllowedIPs[] = "AllowedIPs";
inline constexpr char kEndpoint[] = "Endpoint";
inline constexpr char kIPAddresses[] = "IPAddresses";
inline constexpr char kPeers[] = "Peers";
inline constexpr char kPersistentKeepalive[] = "PersistentKeepalive";
inline constexpr char kPresharedKey[] = "PresharedKey";
inline constexpr char kPrivateKey[] = "PrivateKey";
inline constexpr char kPublicKey[] = "PublicKey";
}  // namespace wireguard

namespace openvpn_compression_algorithm {
inline constexpr char kFramingOnly[] = "FramingOnly";
inline constexpr char kLz4[] = "LZ4";
inline constexpr char kLz4V2[] = "LZ4-V2";
inline constexpr char kLzo[] = "LZO";
inline constexpr char kNone[] = "None";
}  // namespace openvpn_compression_algorithm

namespace openvpn_user_auth_type {
inline constexpr char kNone[] = "None";
inline constexpr char kOTP[] = "OTP";
inline constexpr char kPassword[] = "Password";
inline constexpr char kPasswordAndOTP[] = "PasswordAndOTP";
}  // namespace openvpn_user_auth_type

namespace third_party_vpn {
inline constexpr char kExtensionID[] = "ExtensionID";
inline constexpr char kProviderName[] = "ProviderName";
}  // namespace third_party_vpn

namespace arc_vpn {
// Deprecated. Property left here for ONC backward compatibility. See
// b/185202698 for details.
inline constexpr char kTunnelChrome[] = "TunnelChrome";
}  // namespace arc_vpn

namespace verify_x509 {
inline constexpr char kName[] = "Name";
inline constexpr char kType[] = "Type";

namespace types {
inline constexpr char kName[] = "name";
inline constexpr char kNamePrefix[] = "name-prefix";
inline constexpr char kSubject[] = "subject";
}  // namespace types
}  // namespace verify_x509

namespace substitutes {
inline constexpr char kLoginEmail[] = "LOGIN_EMAIL";
inline constexpr char kLoginID[] = "LOGIN_ID";
inline constexpr char kCertSANEmail[] = "CERT_SAN_EMAIL";
inline constexpr char kCertSANUPN[] = "CERT_SAN_UPN";
inline constexpr char kCertSubjectCommonName[] = "CERT_SUBJECT_COMMON_NAME";
inline constexpr char kDeviceSerialNumber[] = "DEVICE_SERIAL_NUMBER";
inline constexpr char kDeviceAssetId[] = "DEVICE_ASSET_ID";
// The password placeholder is defined as ${PASSWORD} because it's compared
// verbatim against the policy-specified password field, and if it matches,
// another bool (|shill::kEapUseLoginPasswordProperty|) is set, which makes
// shill replace the whole password field.
// The other placeholders above on the other hand are replaced using
// VariableExpander.
inline constexpr char kPasswordPlaceholderVerbatim[] = "${PASSWORD}";
}  // namespace substitutes

namespace proxy {
inline constexpr char kDirect[] = "Direct";
inline constexpr char kExcludeDomains[] = "ExcludeDomains";
inline constexpr char kFtp[] = "FTPProxy";
inline constexpr char kHost[] = "Host";
inline constexpr char kHttp[] = "HTTPProxy";
inline constexpr char kHttps[] = "SecureHTTPProxy";
inline constexpr char kManual[] = "Manual";
inline constexpr char kPAC[] = "PAC";
inline constexpr char kPort[] = "Port";
inline constexpr char kSocks[] = "SOCKS";
inline constexpr char kType[] = "Type";
inline constexpr char kWPAD[] = "WPAD";
}  // namespace proxy

namespace global_network_config {
inline constexpr char kAllowAPNModification[] = "AllowAPNModification";
inline constexpr char kAllowCellularSimLock[] = "AllowCellularSimLock";
inline constexpr char kAllowCellularHotspot[] = "AllowCellularHotspot";
inline constexpr char kAllowOnlyPolicyCellularNetworks[] =
    "AllowOnlyPolicyCellularNetworks";
inline constexpr char kAllowOnlyPolicyNetworksToAutoconnect[] =
    "AllowOnlyPolicyNetworksToAutoconnect";
// AllowOnlyPolicyNetworksToConnect and
// AllowOnlyPolicyNetworksToConnectIfAvailable field are currently only applied
// to WiFi networks. TODO(crbug.com/1234561): Fix this when ONC field is
// updated.
inline constexpr char kAllowOnlyPolicyWiFiToConnect[] =
    "AllowOnlyPolicyNetworksToConnect";
inline constexpr char kAllowOnlyPolicyWiFiToConnectIfAvailable[] =
    "AllowOnlyPolicyNetworksToConnectIfAvailable";
inline constexpr char kAllowTextMessages[] = "AllowTextMessages";
// Deprecated
inline constexpr char kBlacklistedHexSSIDs[] = "BlacklistedHexSSIDs";
inline constexpr char kBlockedHexSSIDs[] = "BlockedHexSSIDs";
inline constexpr char kDisableNetworkTypes[] = "DisableNetworkTypes";
inline constexpr char kRecommendedValuesAreEphemeral[] =
    "RecommendedValuesAreEphemeral";
inline constexpr char kPSIMAdminAssignedAPNIds[] = "PSIMAdminAssignedAPNIds";
inline constexpr char kPSIMAdminAssignedAPNs[] = "PSIMAdminAssignedAPNs";
inline constexpr char kUserCreatedNetworkConfigurationsAreEphemeral[] =
    "UserCreatedNetworkConfigurationsAreEphemeral";
inline constexpr char kDisconnectWiFiOnEthernet[] = "DisconnectWiFiOnEthernet";
inline constexpr char kDisconnectWiFiOnEthernetWhenConnected[] =
    "WhenConnected";
inline constexpr char kDisconnectWiFiOnEthernetWhenOnline[] = "WhenOnline";
}  // namespace global_network_config

namespace device_state {
inline constexpr char kUninitialized[] = "Uninitialized";
inline constexpr char kDisabled[] = "Disabled";
inline constexpr char kEnabling[] = "Enabling";
inline constexpr char kEnabled[] = "Enabled";
}  // namespace device_state

}  // namespace onc

#endif  // COMPONENTS_ONC_ONC_CONSTANTS_H_
