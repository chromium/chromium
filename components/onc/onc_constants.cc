// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/onc/onc_constants.h"

// Constants for ONC properties.
namespace onc {

const char kAugmentationActiveSetting[] = "Active";
const char kAugmentationEffectiveSetting[] = "Effective";
const char kAugmentationUserPolicy[] = "UserPolicy";
const char kAugmentationDevicePolicy[] = "DevicePolicy";
const char kAugmentationUserSetting[] = "UserSetting";
const char kAugmentationSharedSetting[] = "SharedSetting";
const char kAugmentationUserEditable[] = "UserEditable";
const char kAugmentationDeviceEditable[] = "DeviceEditable";

const char kAugmentationActiveExtension[] = "ActiveExtension";

// Common keys/values.
const char kRecommended[] = "Recommended";
const char kRemove[] = "Remove";

// Top Level Configuration
namespace toplevel_config {
const char kAdminAPNList[] = "AdminAPNList";
const char kCertificates[] = "Certificates";
const char kEncryptedConfiguration[] = "EncryptedConfiguration";
const char kNetworkConfigurations[] = "NetworkConfigurations";
const char kGlobalNetworkConfiguration[] = "GlobalNetworkConfiguration";
const char kType[] = "Type";
const char kUnencryptedConfiguration[] = "UnencryptedConfiguration";
}  // namespace toplevel_config

// Network Configuration
namespace network_config {
const char kCellular[] = "Cellular";
const char kCheckCaptivePortal[] = "CheckCaptivePortal";
const char kConnectable[] = "Connectable";
const char kConnectionState[] = "ConnectionState";
const char kDevice[] = "Device";
const char kErrorState[] = "ErrorState";
const char kEthernet[] = "Ethernet";
const char kGUID[] = "GUID";
const char kIPAddressConfigType[] = "IPAddressConfigType";
const char kIPConfigs[] = "IPConfigs";
const char kIPConfigTypeDHCP[] = "DHCP";
const char kIPConfigTypeStatic[] = "Static";
const char kMacAddress[] = "MacAddress";
const char kMetered[] = "Metered";
const char kNameServersConfigType[] = "NameServersConfigType";
const char kName[] = "Name";
const char kPriority[] = "Priority";
const char kProxySettings[] = "ProxySettings";
const char kRestrictedConnectivity[] = "RestrictedConnectivity";
const char kSavedIPConfig[] = "SavedIPConfig";
const char kSourceDevice[] = "Device";
const char kSourceDevicePolicy[] = "DevicePolicy";
const char kSourceNone[] = "None";
const char kSourceUser[] = "User";
const char kSourceUserPolicy[] = "UserPolicy";
const char kSource[] = "Source";
const char kStaticIPConfig[] = "StaticIPConfig";
const char kTether[] = "Tether";
const char kTrafficCounterResetTime[] = "TrafficCounterResetTime";
const char kType[] = "Type";
const char kVPN[] = "VPN";
const char kWiFi[] = "WiFi";
const char kWimaxDeprecated[] = "WiMAX";

std::string CellularProperty(const std::string& property) {
  return std::string(kCellular) + "." + property;
}

std::string TetherProperty(const std::string& property) {
  return std::string(kTether) + "." + property;
}

std::string VpnProperty(const std::string& property) {
  return std::string(kVPN) + "." + property;
}

std::string WifiProperty(const std::string& property) {
  return std::string(kWiFi) + "." + property;
}

}  // namespace network_config

namespace network_type {
const char kAllTypes[] = "All";
const char kCellular[] = "Cellular";
const char kEthernet[] = "Ethernet";
const char kTether[] = "Tether";
const char kVPN[] = "VPN";
const char kWiFi[] = "WiFi";
const char kWimaxDeprecated[] = "WiMAX";
const char kWireless[] = "Wireless";
}  // namespace network_type

namespace check_captive_portal {
const char kFalse[] = "False";
const char kHTTPOnly[] = "HTTPOnly";
const char kTrue[] = "True";
}  // namespace check_captive_portal

namespace cellular {
const char kActivationState[] = "ActivationState";
const char kActivated[] = "Activated";
const char kActivating[] = "Activating";
const char kAutoConnect[] = "AutoConnect";
const char kNotActivated[] = "NotActivated";
const char kPartiallyActivated[] = "PartiallyActivated";
const char kActivationType[] = "ActivationType";
const char kAdminAssignedAPNIds[] = "AdminAssignedAPNIds";
const char kAllowRoaming[] = "AllowRoaming";
const char kAPN[] = "APN";
const char kAPNList[] = "APNList";
const char kCustomAPNList[] = "CustomAPNList";
const char kESN[] = "ESN";
const char kFamily[] = "Family";
const char kFirmwareRevision[] = "FirmwareRevision";
const char kFoundNetworks[] = "FoundNetworks";
const char kHardwareRevision[] = "HardwareRevision";
const char kHomeProvider[] = "HomeProvider";
const char kEID[] = "EID";
const char kICCID[] = "ICCID";
const char kIMEI[] = "IMEI";
const char kIMSI[] = "IMSI";
const char kLastConnectedAttachApnProperty[] = "LastConnectedAttachApnProperty";
const char kLastConnectedDefaultApnProperty[] =
    "LastConnectedDefaultApnProperty";
const char kLastGoodAPN[] = "LastGoodAPN";
const char kManufacturer[] = "Manufacturer";
const char kMDN[] = "MDN";
const char kMEID[] = "MEID";
const char kMIN[] = "MIN";
const char kModelID[] = "ModelID";
const char kNetworkTechnology[] = "NetworkTechnology";
const char kPaymentPortal[] = "PaymentPortal";
const char kRoamingHome[] = "Home";
const char kRoamingRequired[] = "Required";
const char kRoamingRoaming[] = "Roaming";
const char kRoamingState[] = "RoamingState";
const char kScanning[] = "Scanning";
const char kServingOperator[] = "ServingOperator";
const char kSignalStrength[] = "SignalStrength";
const char kSIMLockStatus[] = "SIMLockStatus";
const char kSIMPresent[] = "SIMPresent";
const char kSMDPAddress[] = "SMDPAddress";
const char kSMDSAddress[] = "SMDSAddress";
const char kSupportNetworkScan[] = "SupportNetworkScan";
const char kTechnologyCdma1Xrtt[] = "CDMA1XRTT";
const char kTechnologyEdge[] = "EDGE";
const char kTechnologyEvdo[] = "EVDO";
const char kTechnologyGprs[] = "GPRS";
const char kTechnologyGsm[] = "GSM";
const char kTechnologyHspa[] = "HSPA";
const char kTechnologyHspaPlus[] = "HSPAPlus";
const char kTechnologyLte[] = "LTE";
const char kTechnologyLteAdvanced[] = "LTEAdvanced";
const char kTechnologyUmts[] = "UMTS";
const char kTechnology5gNr[] = "5GNR";
const char kTextMessagesAllow[] = "Allow";
const char kTextMessagesSuppress[] = "Suppress";
const char kTextMessagesUnset[] = "Unset";
}  // namespace cellular

namespace cellular_provider {
const char kCode[] = "Code";
const char kCountry[] = "Country";
const char kName[] = "Name";
}  // namespace cellular_provider

namespace cellular_apn {
const char kAccessPointName[] = "AccessPointName";
const char kName[] = "Name";
const char kUsername[] = "Username";
const char kPassword[] = "Password";
const char kAuthentication[] = "Authentication";
const char kLocalizedName[] = "LocalizedName";
const char kLanguage[] = "Language";
const char kAttach[] = "Attach";
const char kId[] = "Id";
const char kState[] = "State";
const char kStateEnabled[] = "Enabled";
const char kStateDisabled[] = "Disabled";
const char kAuthenticationAutomatic[] = "";
const char kAuthenticationPap[] = "PAP";
const char kAuthenticationChap[] = "CHAP";
const char kIpType[] = "IpType";
const char kIpTypeAutomatic[] = "";
const char kIpTypeIpv4[] = "IPv4";
const char kIpTypeIpv6[] = "IPv6";
const char kIpTypeIpv4Ipv6[] = "IPv4orIPv6";
const char kApnTypes[] = "ApnTypes";
const char kApnTypeDefault[] = "Default";
const char kApnTypeAttach[] = "Attach";
const char kApnTypeTether[] = "Tether";
const char kSource[] = "Source";
const char kSourceModem[] = "Modem";
const char kSourceModb[] = "Modb";
const char kSourceUi[] = "Ui";
const char kSourceAdmin[] = "Admin";
}  // namespace cellular_apn

namespace cellular_found_network {
const char kStatus[] = "Status";
const char kNetworkId[] = "NetworkId";
const char kShortName[] = "ShortName";
const char kLongName[] = "LongName";
const char kTechnology[] = "Technology";
}  // namespace cellular_found_network

namespace cellular_payment_portal {
const char kMethod[] = "Method";
const char kPostData[] = "PostData";
const char kUrl[] = "Url";
}  // namespace cellular_payment_portal

namespace sim_lock_status {
const char kLockEnabled[] = "LockEnabled";
const char kLockType[] = "LockType";
const char kRetriesLeft[] = "RetriesLeft";
}  // namespace sim_lock_status

namespace connection_state {
const char kConnected[] = "Connected";
const char kConnecting[] = "Connecting";
const char kNotConnected[] = "NotConnected";
}  // namespace connection_state

namespace ethernet {
const char kAuthentication[] = "Authentication";
const char kAuthenticationNone[] = "None";
const char kEAP[] = "EAP";
const char k8021X[] = "8021X";
}  // namespace ethernet

namespace tether {
const char kBatteryPercentage[] = "BatteryPercentage";
const char kCarrier[] = "Carrier";
const char kHasConnectedToHost[] = "HasConnectedToHost";
const char kSignalStrength[] = "SignalStrength";
}  // namespace tether

namespace ipconfig {
const char kGateway[] = "Gateway";
const char kIPAddress[] = "IPAddress";
const char kIPv4[] = "IPv4";
const char kIPv6[] = "IPv6";
const char kNameServers[] = "NameServers";
const char kRoutingPrefix[] = "RoutingPrefix";
const char kSearchDomains[] = "SearchDomains";
const char kIncludedRoutes[] = "IncludedRoutes";
const char kExcludedRoutes[] = "ExcludedRoutes";
const char kType[] = "Type";
const char kWebProxyAutoDiscoveryUrl[] = "WebProxyAutoDiscoveryUrl";
const char kMTU[] = "MTU";
}  // namespace ipconfig

namespace wifi {
const char kAllowGatewayARPPolling[] = "AllowGatewayARPPolling";
const char kAutoConnect[] = "AutoConnect";
const char kBSSID[] = "BSSID";
const char kBSSIDAllowlist[] = "BSSIDAllowlist";
const char kBSSIDRequested[] = "BSSIDRequested";
const char kEAP[] = "EAP";
const char kFrequency[] = "Frequency";
const char kFrequencyList[] = "FrequencyList";
const char kHexSSID[] = "HexSSID";
const char kHiddenSSID[] = "HiddenSSID";
const char kPassphrase[] = "Passphrase";
const char kSSID[] = "SSID";
const char kSecurity[] = "Security";
const char kSecurityNone[] = "None";
const char kSignalStrength[] = "SignalStrength";
const char kSignalStrengthRssi[] = "SignalStrengthRssi";
const char kWEP_8021X[] = "WEP-8021X";
const char kWEP_PSK[] = "WEP-PSK";
const char kWPA_EAP[] = "WPA-EAP";
const char kWPA_PSK[] = "WPA-PSK";
const char kWPA2_PSK[] = "WPA2-PSK";
const char kPasspointId[] = "PasspointId";
const char kPasspointMatchType[] = "PasspointMatchType";
}  // namespace wifi

// Deprecated, properties exist for ignoring old ONC config entries.
namespace wimax_deprecated {
const char kAutoConnect[] = "AutoConnect";
const char kEAP[] = "EAP";
}  // namespace wimax_deprecated

namespace client_cert {
const char kClientCertPattern[] = "ClientCertPattern";
const char kClientCertPKCS11Id[] = "ClientCertPKCS11Id";
const char kClientCertProvisioningProfileId[] =
    "ClientCertProvisioningProfileId";
const char kClientCertRef[] = "ClientCertRef";
const char kClientCertType[] = "ClientCertType";
const char kClientCertTypeNone[] = "None";
const char kCommonName[] = "CommonName";
const char kEmailAddress[] = "EmailAddress";
const char kEnrollmentURI[] = "EnrollmentURI";
const char kIssuerCARef[] = "IssuerCARef";
const char kIssuerCAPEMs[] = "IssuerCAPEMs";
const char kIssuer[] = "Issuer";
const char kLocality[] = "Locality";
const char kOrganization[] = "Organization";
const char kOrganizationalUnit[] = "OrganizationalUnit";
const char kPattern[] = "Pattern";
const char kProvisioningProfileId[] = "ProvisioningProfileId";
const char kPKCS11Id[] = "PKCS11Id";
const char kRef[] = "Ref";
const char kSubject[] = "Subject";
}  // namespace client_cert

namespace certificate {
const char kAuthority[] = "Authority";
const char kClient[] = "Client";
const char kGUID[] = "GUID";
const char kPKCS12[] = "PKCS12";
const char kScope[] = "Scope";
const char kServer[] = "Server";
const char kTrustBits[] = "TrustBits";
const char kType[] = "Type";
const char kWeb[] = "Web";
const char kX509[] = "X509";
}  // namespace certificate

namespace scope {
const char kDefault[] = "Default";
const char kExtension[] = "Extension";
const char kId[] = "Id";
const char kType[] = "Type";
}  // namespace scope

namespace encrypted {
const char kAES256[] = "AES256";
const char kCipher[] = "Cipher";
const char kCiphertext[] = "Ciphertext";
const char kHMACMethod[] = "HMACMethod";
const char kHMAC[] = "HMAC";
const char kIV[] = "IV";
const char kIterations[] = "Iterations";
const char kPBKDF2[] = "PBKDF2";
const char kSHA1[] = "SHA1";
const char kSalt[] = "Salt";
const char kStretch[] = "Stretch";
}  // namespace encrypted

namespace eap {
const char kAnonymousIdentity[] = "AnonymousIdentity";
const char kAutomatic[] = "Automatic";
const char kCHAP[] = "CHAP";
const char kDomainSuffixMatch[] = "DomainSuffixMatch";
const char kEAP_AKA[] = "EAP-AKA";
const char kEAP_FAST[] = "EAP-FAST";
const char kEAP_SIM[] = "EAP-SIM";
const char kEAP_TLS[] = "EAP-TLS";
const char kEAP_TTLS[] = "EAP-TTLS";
const char kGTC[] = "GTC";
const char kIdentity[] = "Identity";
const char kInner[] = "Inner";
const char kLEAP[] = "LEAP";
const char kMD5[] = "MD5";
const char kMSCHAP[] = "MSCHAP";
const char kMSCHAPv2[] = "MSCHAPv2";
const char kOuter[] = "Outer";
const char kPAP[] = "PAP";
const char kPEAP[] = "PEAP";
const char kPassword[] = "Password";
const char kSaveCredentials[] = "SaveCredentials";
const char kServerCAPEMs[] = "ServerCAPEMs";
const char kServerCARef[] = "ServerCARef";
const char kServerCARefs[] = "ServerCARefs";
const char kSubjectMatch[] = "SubjectMatch";
const char kSubjectAlternativeNameMatch[] = "SubjectAlternativeNameMatch";
const char kTLSVersionMax[] = "TLSVersionMax";
const char kUseSystemCAs[] = "UseSystemCAs";
const char kUseProactiveKeyCaching[] = "UseProactiveKeyCaching";
}  // namespace eap

namespace eap_subject_alternative_name_match {
const char kType[] = "Type";
const char kValue[] = "Value";
const char kEMAIL[] = "EMAIL";
const char kDNS[] = "DNS";
const char kURI[] = "URI";
}  // namespace eap_subject_alternative_name_match

namespace vpn {
const char kArcVpn[] = "ARCVPN";
const char kAutoConnect[] = "AutoConnect";
const char kHost[] = "Host";
const char kIPsec[] = "IPsec";
const char kL2TP[] = "L2TP";
const char kOpenVPN[] = "OpenVPN";
const char kPassword[] = "Password";
const char kSaveCredentials[] = "SaveCredentials";
const char kThirdPartyVpn[] = "ThirdPartyVPN";
const char kTypeL2TP_IPsec[] = "L2TP-IPsec";
const char kType[] = "Type";
const char kUsername[] = "Username";
const char kWireGuard[] = "WireGuard";
}  // namespace vpn

namespace ipsec {
const char kAuthenticationType[] = "AuthenticationType";
const char kCert[] = "Cert";
const char kEAP[] = "EAP";
const char kGroup[] = "Group";
const char kIKEVersion[] = "IKEVersion";
const char kLocalIdentity[] = "LocalIdentity";
const char kPSK[] = "PSK";
const char kRemoteIdentity[] = "RemoteIdentity";
const char kServerCAPEMs[] = "ServerCAPEMs";
const char kServerCARef[] = "ServerCARef";
const char kServerCARefs[] = "ServerCARefs";
const char kXAUTH[] = "XAUTH";
}  // namespace ipsec

namespace l2tp {
const char kLcpEchoDisabled[] = "LcpEchoDisabled";
const char kPassword[] = "Password";
const char kSaveCredentials[] = "SaveCredentials";
const char kUsername[] = "Username";
}  // namespace l2tp

namespace openvpn {
const char kAuthNoCache[] = "AuthNoCache";
const char kAuthRetry[] = "AuthRetry";
const char kAuth[] = "Auth";
const char kCipher[] = "Cipher";
const char kCompLZO[] = "CompLZO";
const char kCompNoAdapt[] = "CompNoAdapt";
const char kCompressionAlgorithm[] = "CompressionAlgorithm";
const char kExtraHosts[] = "ExtraHosts";
const char kIgnoreDefaultRoute[] = "IgnoreDefaultRoute";
const char kInteract[] = "interact";
const char kKeyDirection[] = "KeyDirection";
const char kNoInteract[] = "nointeract";
const char kNone[] = "none";
const char kNsCertType[] = "NsCertType";
const char kOTP[] = "OTP";
const char kPassword[] = "Password";
const char kPort[] = "Port";
const char kProto[] = "Proto";
const char kPushPeerInfo[] = "PushPeerInfo";
const char kRemoteCertEKU[] = "RemoteCertEKU";
const char kRemoteCertKU[] = "RemoteCertKU";
const char kRemoteCertTLS[] = "RemoteCertTLS";
const char kRenegSec[] = "RenegSec";
const char kServerCAPEMs[] = "ServerCAPEMs";
const char kServerCARef[] = "ServerCARef";
const char kServerCARefs[] = "ServerCARefs";
const char kServerCertPEM[] = "ServerCertPEM";
const char kServerCertRef[] = "ServerCertRef";
const char kServerPollTimeout[] = "ServerPollTimeout";
const char kServer[] = "server";
const char kShaper[] = "Shaper";
const char kStaticChallenge[] = "StaticChallenge";
const char kTLSAuthContents[] = "TLSAuthContents";
const char kTLSRemote[] = "TLSRemote";
const char kTLSVersionMin[] = "TLSVersionMin";
const char kUserAuthenticationType[] = "UserAuthenticationType";
const char kVerb[] = "Verb";
const char kVerifyHash[] = "VerifyHash";
const char kVerifyX509[] = "VerifyX509";
}  // namespace openvpn

namespace openvpn_compression_algorithm {
const char kFramingOnly[] = "FramingOnly";
const char kLz4[] = "LZ4";
const char kLz4V2[] = "LZ4-V2";
const char kLzo[] = "LZO";
const char kNone[] = "None";
}  // namespace openvpn_compression_algorithm

namespace openvpn_user_auth_type {
const char kNone[] = "None";
const char kOTP[] = "OTP";
const char kPassword[] = "Password";
const char kPasswordAndOTP[] = "PasswordAndOTP";
}  // openvpn_user_auth_type

namespace wireguard {
const char kAllowedIPs[] = "AllowedIPs";
const char kEndpoint[] = "Endpoint";
const char kIPAddresses[] = "IPAddresses";
const char kPeers[] = "Peers";
const char kPersistentKeepalive[] = "PersistentKeepalive";
const char kPresharedKey[] = "PresharedKey";
const char kPrivateKey[] = "PrivateKey";
const char kPublicKey[] = "PublicKey";
}  // namespace wireguard

namespace third_party_vpn {
const char kExtensionID[] = "ExtensionID";
const char kProviderName[] = "ProviderName";
}  // third_party_vpn

namespace arc_vpn {
// Deprecated. Property left here for ONC backward compatibility. See
// b/185202698 for details.
const char kTunnelChrome[] = "TunnelChrome";
}  // namespace arc_vpn

namespace verify_x509 {
const char kName[] = "Name";
const char kType[] = "Type";

namespace types {
const char kName[] = "name";
const char kNamePrefix[] = "name-prefix";
const char kSubject[] = "subject";
}  // namespace types
}  // namespace verify_x509

namespace proxy {
const char kDirect[] = "Direct";
const char kExcludeDomains[] = "ExcludeDomains";
const char kFtp[] = "FTPProxy";
const char kHost[] = "Host";
const char kHttp[] = "HTTPProxy";
const char kHttps[] = "SecureHTTPProxy";
const char kManual[] = "Manual";
const char kPAC[] = "PAC";
const char kPort[] = "Port";
const char kSocks[] = "SOCKS";
const char kType[] = "Type";
const char kWPAD[] = "WPAD";
}  // namespace proxy

namespace substitutes {
const char kLoginID[] = "LOGIN_ID";
const char kLoginEmail[] = "LOGIN_EMAIL";
const char kCertSANEmail[] = "CERT_SAN_EMAIL";
const char kCertSANUPN[] = "CERT_SAN_UPN";
const char kCertSubjectCommonName[] = "CERT_SUBJECT_COMMON_NAME";
const char kDeviceSerialNumber[] = "DEVICE_SERIAL_NUMBER";
const char kDeviceAssetId[] = "DEVICE_ASSET_ID";
// The password placeholder is defined as ${PASSWORD} because it's compared
// verbatim against the policy-specified password field, and if it matches,
// another bool (|shill::kEapUseLoginPasswordProperty|) is set, which makes
// shill replace the whole password field.
// The other placeholders above on the other hand are replaced using
// VariableExpander.
const char kPasswordPlaceholderVerbatim[] = "${PASSWORD}";
}  // namespace substitutes

namespace global_network_config {
const char kAllowAPNModification[] = "AllowAPNModification";
const char kAllowCellularSimLock[] = "AllowCellularSimLock";
const char kAllowCellularHotspot[] = "AllowCellularHotspot";
const char kAllowOnlyPolicyCellularNetworks[] =
    "AllowOnlyPolicyCellularNetworks";
const char kAllowOnlyPolicyNetworksToAutoconnect[] =
    "AllowOnlyPolicyNetworksToAutoconnect";
const char kAllowOnlyPolicyNetworksToConnect[] =
    "AllowOnlyPolicyNetworksToConnect";
const char kAllowOnlyPolicyNetworksToConnectIfAvailable[] =
    "AllowOnlyPolicyNetworksToConnectIfAvailable";
// AllowOnlyPolicyNetworksToConnect and
// AllowOnlyPolicyNetworksToConnectIfAvailable field are currently only applied
// to WiFi networks. TODO(crbug.com/1234561): Fix this when ONC field is
// updated.
const char* const kAllowOnlyPolicyWiFiToConnect =
    kAllowOnlyPolicyNetworksToConnect;
const char* const kAllowOnlyPolicyWiFiToConnectIfAvailable =
    kAllowOnlyPolicyNetworksToConnectIfAvailable;
const char* const kAllowTextMessages = "AllowTextMessages";

const char kBlacklistedHexSSIDs[] = "BlacklistedHexSSIDs";  // Deprecated
const char kBlockedHexSSIDs[] = "BlockedHexSSIDs";
const char kDisableNetworkTypes[] = "DisableNetworkTypes";
const char kPSIMAdminAssignedAPNIds[] = "PSIMAdminAssignedAPNIds";
const char kPSIMAdminAssignedAPNs[] = "PSIMAdminAssignedAPNs";
const char kRecommendedValuesAreEphemeral[] = "RecommendedValuesAreEphemeral";
const char kUserCreatedNetworkConfigurationsAreEphemeral[] =
    "UserCreatedNetworkConfigurationsAreEphemeral";
const char kDisconnectWiFiOnEthernet[] = "DisconnectWiFiOnEthernet";
const char kDisconnectWiFiOnEthernetWhenConnected[] = "WhenConnected";
const char kDisconnectWiFiOnEthernetWhenOnline[] = "WhenOnline";
}  // global_network_config

namespace device_state {
const char kUninitialized[] = "Uninitialized";
const char kDisabled[] = "Disabled";
const char kEnabling[] = "Enabling";
const char kEnabled[] = "Enabled";
}  // device_state

}  // namespace onc
