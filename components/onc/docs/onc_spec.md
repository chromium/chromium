# Open Network Configuration

[TOC]

## Objective

We would like to create a simple, open, but complete format to describe
multiple network configurations for WiFi, Ethernet, Cellular,
Bluetooth/WiFi-Direct, and VPN connections in a single file format, in order
to simplify and automate network configuration for users.

## Background

Configuring networks is a painful and error-prone experience for users. It
is a problem shared across desktop, laptop, tablet, and phone users of all
operating system types. It is exacerbated in business and schools which
often have complex network configurations (VPNs and 802.1X networking) that
change often and have many connected devices. Configuration of WiFi is
still done manually, often by administrators physically standing next to
users working on devices. Certificate distribution is particularly painful
which often results in admins instead using passphrases to protect networks
or using protocols without client certificates that instead use LDAP
passwords for authentication. Even after networks are configured, updates to
the network configuration require another round of manual changes, and
accidental changes by a user or malicious changes by an attacker can break
connectivity or make connections less private or secure.

## Overview

We propose a single-file format for network configuration that is
human-readable, can describe all of the common kinds of network
configurations, supports integrity checking, certificate and key
provisioning, and updating. The file can be encrypted with a single
passphrase so that upon entering the passphrase the entire configuration is
loaded. The format can be described as an open format to enable multiple OS
vendors to interoperate and share configuration editors.

This format neither supports configuring browser settings nor allows setting
other types of system policies.

## Infrastructure

A standalone configuration editor will be created, downloadable as a Chrome
app. This editor will allow creating, modifying, and encrypting an open
network configuration file in a way that is intuitive for a system
administrator.

This file format may be delivered to a user and manually imported into a
device.

This file format may be created by an administrator, stored in a policy
repository, and automatically pushed to a device.

## Detailed Design

We use JSON format for the files. The fields in a JSON file are always
case-sensitive, so the exact case of the fields in this section must be
matched. In addition, the values that are called out as explicit constants
must also match the case specified (e.g. WiFi must not be written as wifi,
etc.). This document describes a minimum set of required fields and optional
fields. Other fields may be created, however, see the
implementation-specific fields for guidelines for these fields.

The JSON consists of a top level dictionary containing
a `Type` field which must have either the value
[EncryptedConfiguration](#EncryptedConfiguration-type) or
[UnencryptedConfiguration](#UnencryptedConfiguration-type).

For a description of the [EncryptedConfiguration](#EncryptedConfiguration-type)
type, see the section on [Encrypted Configuration](#Encrypted-Configuration)
below.
The [EncryptedConfiguration](#Encrypted-Configuration) format encrypts
an unencrypted JSON object.

## GUIDs and Updating

This format allows for importing updated network configurations and
certificates by providing GUIDs to each network configuration and
certificate so they can be modified or even removed in future updates.

GUIDs are non-empty strings that are meant to be stable and unique. When
they refer to the same entity, they should be the same between ONC files. No
two different networks or certificates should have the same GUID, similarly
a network and certificate should not have the same GUID. A single ONC file
should not contain the same entity twice (with the same GUID). Failing any
of these tests indicates the ONC file is not valid.

Any GUID referred to in an ONC file must be present in the same ONC file. In
particular, it is an error to create a certificate in one ONC file and refer
to it in a NetworkConfiguration in another ONC file and not define it there,
even if the previous ONC file has been imported.

## Implementation-specific fields

As there are many different kinds of connections and some that are not yet
anticipated may require new fields. This format allows arbitrary other
fields to be added.

Fields and values should follow these general guidelines:

* Certificates (with and without keys) should always be placed in the
  certificate section - specifically certificate contents should not be
  placed in fields directly. Referring to certificates should be done using
  a field whose name ends in Ref and whose value is the GUID of the
  certificate, or if the certificate is not contained in this file, its
  pattern can be described using a field ending in Pattern of
  [CertificatePattern](#CertificatePattern-type) type.
* Fields should exist in the most-specific object in the hierarchy and
  should be named CamelCase style.
* Booleans and integers should be used directly instead of using a
  stringified version of the type.

Any editor of network configuration information should allows the user to
modify any fields that are implementation-specific. It may not be present
directly in the UI but it should be able to import files with such settings
and leave preserve these settings on export.

## Unencrypted Configuration

When the top level **Type** field
is *UnencryptedConfiguration*, the top level JSON
has the [UnencryptedConfiguration](#UnencryptedConfiguration-type) type.

### UnencryptedConfiguration type

* **Type**
    * (optional, defaults to *UnencryptedConfiguration*) - **string**
    * Must be *UnencryptedConfiguration*.

* **NetworkConfigurations**
    * (optional) - [array of NetworkConfiguration](#Network-Configuration)
    * Describes WiFi, Ethernet, VPN, and wireless connections.

* **Certificates**
    * (optional) - [array of Certificate](#Certificate-type)
    * Contains certificates stored in X.509 or PKCS#12 format.

---
  * At least one actual configuration field
    (**NetworkConfigurations** or **Certificates**) should be present,
    however it should not be considered an error if no such field is present.
---

## Global Network Configuration

Field **GlobaNetworkConfiguration** has the [GlobalNetworkConfiguration]
(#GlobalNetworkConfiguration-type) type.

### GlobalNetworkConfiguration type

The [GlobalNetworkConfiguration](#GlobalNetworkConfiguration-type) contains
settings which apply to all of the networks that the device may connect to. The
client supports this only in device-level policy; the client-side ONC validator
fails if it appears in user policy.
To avoid bricking devices, these policies will only be enforced in user
sessions. The login screen ignores these policies and may still be used for
fetching new policy or logging in.
A [Help Center article](https://support.google.com/chrome/a/answer/6326250)
warns admins of the implications of mis-using this policy for Chrome OS.


* **AllowOnlyPolicyNetworksToAutoconnect**
    * (optional, defaults to false) - **boolean**
    * When this field is present and set to true, on startup the device will
      only auto connect to those networks that are present in its policy. This
      is necessary for devices that are used as kiosks, for example, so that
      they can’t be hijacked by some other user on startup.

* **AllowOnlyPolicyNetworksToConnect**
    * (optional, defaults to false) - **boolean**
    * When this field is present and set to true, only networks present in
      policy may be connected to. This allows schools to enforce that only
      known-good networks (e.g., filtered student networks) may be used.
      Existing connections to unmanaged networks will be disconnected on policy
      fetch.

* **AllowOnlyPolicyNetworksToConnectIfAvailable**
    * (optional, defaults to false) - **boolean**
    * When this field is present, set to true and a policy network is in range,
      only policy networks may be connected to. If no managed network is in
      range (e.g. user’s home), the device may connect to any network. If
      enabled and a network scan shows a new policy managed network, the device
      will automatically switch to the managed network.

* **BlacklistedHexSSIDs**
    * (optional) - **array of string**
    * List of strings containing blacklisted hex SSIDs. Networks included in
      this list will not be connectable. Existing connections to networks
      contained in this list will be disconnected on policy fetch.

* **DisableNetworkTypes**
    * (optional) - **array of string**
    * Allowed values are:
        * Cellular
        * Ethernet
        * WiFi
        * Tether
    * List of strings containing disabled network interfaces.

## Network Configuration

Field **NetworkConfigurations** is an array of
[NetworkConfiguration](#NetworkConfiguration-type) typed objects.

### NetworkConfiguration type

* **Ethernet**
    * (required if **Type** is *Ethernet*, otherwise ignored) -
      [Ethernet](#Ethernet-type)
    * Ethernet settings.

* **GUID**
    * (required) - **string**
    * A unique identifier for this network connection, which exists to make it
      possible to update previously imported configurations. Must be a non-empty
      string.

* **IPAddressConfigType**
    * (optional if **Remove** is *false*, otherwise ignored. Defaults to *DHCP*
      if **NameServersConfigType** is specified) - **string**
    * Allowed values are:
        * *DHCP*
        * *Static*
    * Determines whether the IP Address configuration is statically configured,
      see **StaticIPConfig**, or automatically configured
      using DHCP.

* **NameServersConfigType**
    * (optional if **Remove** is *false*, otherwise ignored. Defaults to *DHCP*
    if **IPAddressConfigType** is specified) - **string**
    * Allowed values are:
        * *DHCP*
        * *Static*
    * Determines whether the NameServers configuration is statically configured,
      see **StaticIPConfig**, or automatically configured
      using DHCP.

* **IPConfigs**
    * (optional for connected networks, read-only) -
      [array of IPConfig](#IPConfig-type)
    * Array of IPConfig properties associated with this connection.

* **StaticIPConfig**
    * (required if **IPAddressConfigType** or **NameServersConfigType** is set
      to *Static*) - [IPConfig](#IPConfig-type)
    * Each property set in this IPConfig object overrides the respective
      parameter received over DHCP. If **IPAddressConfigType** is set to
      *Static*, **IPAddress**, **Gateway** and **RoutingPrefix** are required.
      If **NameServersConfigType** is set to *Static*, **NameServers** is
      required.

* **SavedIPConfig**
    * (optional for connected networks, read-only) - [IPConfig](#IPConfig-type)
    * IPConfig property containing the configuration that was received from the
      DHCP server prior to applying any StaticIPConfig parameters.

* **Name**
    * (required if **Remove** is *false*, otherwise ignored) - **string**
    * A user-friendly description of this connection. This name will not be used
      for referencing and may not be unique. Instead it may be used for
      describing the network to the user.

* **Remove**
    * (optional, defaults to *false*) - **boolean**
    * If set, remove this network configuration (only GUID should be set).

* **ProxySettings**
    * (optional if **Remove** is *false*, otherwise ignored) -
      [ProxySettings](#ProxySettings-type)
    * Proxy settings for this network

* **VPN**
    * (required if **Type** is *VPN*, otherwise ignored) - [VPN](#VPN-type)
    * VPN settings.

* **WiFi**
    * (required if **Type** is *WiFi*, otherwise ignored) - [WiFi](#WiFi-type)
    * WiFi settings.

* **Cellular**
    * (required if **Type** is *Cellular*, otherwise ignored) -
      [Cellular](#Cellular-type)
    * Cellular settings.

* **Tether**
    * (required if **Type** is *Tether*, otherwise ignored) -
      [Tether](#Tether-type)
    * Tether settings.

* **Type**
    * (required if **Remove** is *false*, otherwise ignored) - **string**
    * Allowed values are:
        * *Cellular*
        * *Ethernet*
        * *WiFi*
        * *VPN*
    * Indicates which kind of connection this is.

* **ConnectionState**
    * (optional, read-only) - **string**
    * The current connection state for this network, provided by the system.
      Allowed values are:
        * *Connected*
        * *Connecting*
        * *NotConnected*

* **RestrictedConnectivity**
    * (optional, defaults to *false*, read-only) - **boolean**
    * True if a connnected network has limited connectivity to the Internet,
      e.g. a connection is behind a portal or a cellular network is not
      activated or requires payment.

* **Connectable**
    * (optional, read-only) - **boolean**
    * True if the system indicates that the network can be connected to without
      any additional configuration.

* **ErrorState**
    * (optional, read-only) - **string**
    * The current error state for this network, if any. Error states are
      provided by the system and are not explicitly defined here. They may or
      may not be human-readable. This field will be empty or absent if the
      network is not in an error state.

* **MacAddress**
    * (optional, read-only) - **string**
    * The MAC address for the network. Only applies to connected non-virtual
      networks. The format is 00:11:22:AA:BB:CC.

* **Source**
    * (optional, read-only) - **string**
    * Indicates whether the network is configured and how it is configured:
        * *User*: Configured for the active
          user only, i.e. an unshared configuration.
        * *Device*: Configured for all users of the
          device (e.g laptop), i.e. a shared configuration.
        * *UserPolicy*: Configured by the user
          policy for the active user.
        * *DevicePolicy*: Configured by the device
          policy for the device.
        * *None*: Not configured, e.g. a visible
          but unconfigured WiFi network.
    * Allowed values are:
        * *User*,
        * *Device*,
        * *UserPolicy*,
        * *DevicePolicy*,
        * *None*

* **Priority**
    * (optional) - **integer**
    * Provides a suggested priority value for this network. May be used by the
      system to determine which network to connect to when multiple configured
      networks are available (or may be ignored).


## Ethernet networks

For Ethernet connections, **Type** must be set to
*Ethernet* and the field **Ethernet** must be set to an object of
type [Ethernet](#Ethernet-type).

### Ethernet type

* **Authentication**
    * (optional) - **string**
    * Allowed values are:
        * *None*
        * *8021X*

* **EAP**
    * (required if **Authentication** is *8021X*, otherwise ignored) -
      [EAP](#EAP-type)
    * EAP settings.

## IPConfig

Objects of type [IPConfig](#IPConfig-type) are used to report the
actual IP configuration of a connected network (see
**IPConfigs**), the IP configuration received from
DHCP (see **SavedIPConfig**) and to configure a
static IP configuration (see **StaticIPConfig**).

### IPConfig type

* **Type**
    * (optional, defaults to *IPv4*) - **string**
    * Allowed values are:
        * *IPv4*
        * *IPv6*
    * Describes the type of configuration this is.

* **IPAddress**
    * (optional) - **string**
    * Describes the IPv4 or IPv6 address of a connection, depending on the value
      of **Type** field. It should not contain the routing prefix (i.e. should
      not end in something like /64).

* **RoutingPrefix**
    * (required if **IPAddress** is set. Otherwise ignored.) - **integer**
    * `Must be a number in the range [1, 32] for IPv4 and [1, 128] for
      IPv6 addresses.`
    * Describes the routing prefix.

* **Gateway**
    * (required if **IPAddress** is set. Otherwise ignored.) - **string**
    * Describes the gateway address to use for the configuration. Must match
      address type specified in **Type** field. If not
      specified, DHCP values will be used.

* **NameServers**
    * (optional) - **array of string**
    * Array of addresses to use for name servers. If not specified, DHCP values
    will be used.

* **SearchDomains**
    * (optional) - **array of string**
    * Array of strings to append to names for resolution. Items in this array
      should not start with a dot. Example: `["corp.acme.org", "acme.org" ]`.
      If not specified, DHCP values will be used.

* **IncludedRoutes**
    * (optional) - **array of string**
    * An array of strings, each of which is an IP block in CIDR notation,
      whose traffic should be handled by the network. Example:
      `["10.0.0.0/8", "192.168.5.0/24"]`. If **IncludedRoutes** or
      **ExcludedRoutes** are not specified, this network
      will be used to handle traffic for all IPs by default. Currently this
      property only has an effect if the Network **Type** is *VPN* and the
      VPN **Type** is *ARCVPN*.

* **ExcludedRoutes**
    * (optional) - **array of string**
    * An array of strings, each of which is an IP block in CIDR notation,
      whose traffic should **not** be handled by the network. Example:
      `["10.0.0.0/8", "192.168.5.0/24"]`. Currently this
      only has an effect if the Network **Type** is *VPN* and the VPN
      **Type** is *ARCVPN*.

* **WebProxyAutoDiscoveryUrl**
    * (optional if part of **IPConfigs**, read-only) - **string**
    * The Web Proxy Auto-Discovery URL for this network as reported over DHCP.


## WiFi networks

For WiFi connections, **Type** must be set to *WiFi* and the
field **WiFi** must be set to an object of type [WiFi](#WiFi-type).

### WiFi type

* **AllowGatewayARPPolling**
    * (optional, defaults to *true*) - **boolean**
    * Indicaties if ARP polling of default gateway is allowed.
      When it is allowed, periodic ARP messages will be sent to
      the default gateway. This is used for monitoring the status
      of the current connection.

* **AutoConnect**
    * (optional, defaults to *false*) - **boolean**
    * Indicating that the network should be connected to automatically when in
      range.

* **EAP**
    * (required if **Security** is
        *WEP-8021X* or *WPA-EAP*, otherwise ignored) - [EAP](#EAP-type)
    * EAP settings.

* **FTEnabled**
    * (optional, defaults to *false*) - **boolean**
    * Indicating if the client should attempt to use Fast Transition with the
    * network.

* **HexSSID**
    * (optional if **SSID** is set, if so defaults to a hex representation of
      **SSID**) - **string**
    * Hex representation of the network's SSID.

* **HiddenSSID**
    * (optional, defaults to *false*) - **boolean**
    * Indicating if the SSID will be broadcast.

* **Passphrase**
    * (required if **Security** is
        *WEP-PSK* or *WPA-PSK*, otherwise ignored) - **string**
    * Describes the passphrase for WEP/WPA/WPA2
      connections. If *WEP-PSK* is used, the passphrase
      must be of the format 0x&lt;hex-number&gt;, where &lt;hex-number&gt; is
      40, 104, 128, or 232 bits.

* **RoamThreshold**
    * (optional) - **integer**
    * The roam threshold for this network, which is the signal-to-noise value
      (in dB) below which we will attempt to roam to a new network. If this
      value is not set, the default value will be used.

* **Security**
    * (required) - **string**
    * Allowed values are:
        * *None*
        * *WEP-PSK*
        * *WEP-8021X*
        * *WPA-PSK*
        * *WPA-EAP*

* **SSID**
    * (optional if **HexSSID** is set, otherwise ignored) - **string**
    * Property to access the decoded SSID of a network.<br/>
      If this field is set, but **HexSSID** is not,
      its value will be UTF-8 encoded and the hex representation will be
      assigned to **HexSSID**. To configure a non-UTF-8
      SSID, field **HexSSID** must be used.<br/>
      When reading the configuration of a network, both this field and
      **HexSSID** might be set. Then this field is the
      decoding of **HexSSID**. If possible the HexSSID is
      decoded using UTF-8, otherwise an encoding is guessed on a best effort
      basis.

* **SignalStrength**
    * (optional, read-only) - **integer**
    * The current signal strength for this network in the range [0, 100],
      provided by the system. If the network is not in range this field will
      be set to '0' or not present.

* **TetheringState**
    * (optional, read-only, defaults to "NotDetected") - **string**
    * The tethering state of the WiFi connection. If the connection is
      tethered the value is "Confirmed". If the connection is suspected to be
      tethered the value is "Suspected". In all other cases it's
      "NotDetected".

---
  * At least one of the fields **HexSSID** or **SSID** must be present.
  * If both **HexSSID** and **SSID** are set, the values must be consistent.
---

## VPN networks

There are many kinds of VPNs with widely varying configuration options. We
offer standard configuration options for a few common configurations at this
time, and may add more later. For all others, implementation specific fields
should be used.

For VPN connections, **Type** must be set to *VPN* and the
field **VPN** must be set to an object of type [VPN](#VPN-type).

### VPN type

* **AutoConnect**
    * (optional, defaults to *false*) - **boolean**
    * Indicating that the network should be connected to automatically.

* **Host**
    * (optional) - **string**
    * Host name or IP address of server to connect to. The only scenario that
      does not require a host is a VPN that encrypts but does not tunnel
      traffic. Standalone IPsec (v1 or v2, cert or PSK based -- this is not the
      same as L2TP over IPsec) is one such setup. For all other types of VPN,
      the **Host** field is required.

* **IPsec**
    * (required if **Type** is
        *IPsec* or *L2TP-IPsec*, otherwise ignored) - [IPsec](#IPsec-type)
    * IPsec layer settings.

* **L2TP**
    * (required if **Type** is *L2TP-IPsec*, otherwise ignored) -
      [L2TP](#L2TP-type)
    * L2TP layer settings.

* **OpenVPN**
    * (required if **Type** is *OpenVPN*, otherwise ignored) -
      [OpenVPN](#OpenVPN-type)
    * OpenVPN settings.

* **ThirdPartyVPN**
    * (required if **Type** is *ThirdPartyVPN*, otherwise ignored) -
      [ThirdPartyVPN](#ThirdPartyVPN-type)
    * Third-party VPN provider settings.

* **Type**
    * (required) - **string**
    * Allowed values are:
        * *ARCVPN*
        * *L2TP-IPsec*
        * *OpenVPN*
        * *ThirdPartyVPN*
    * Type of the VPN.

## IPsec-based VPN types

### IPsec type

* **AuthenticationType**
    * (required) - **string**
    * Allowed values are:
        * *Cert*
        * *PSK*
    * If *Cert* is used, **ClientCertType** and *ServerCARefs* (or the
      deprecated *ServerCARef*) must be set.

* **ClientCertPKCS11Id**
    * (required if **ClientCertType** is *PKCS11Id*, otherwise ignored) -
    * PKCS#11 identifier in the format slot:key_id.

* **ClientCertPattern**
    * (required if **ClientCertType** is *Pattern*, otherwise ignored) -
      [CertificatePattern](#CertificatePattern-type)
    * Pattern describing the client certificate.

* **ClientCertRef**
    * (required if **ClientCertType** is *Ref*, otherwise ignored) - **string**
    * Reference to client certificate stored in certificate section.

* **ClientCertType**
    * (required if **AuthenticationType** is *Cert*, otherwise ignored) -
      **string**
    * Allowed values are
      * *PKCS11Id*
      * *Pattern*
      * *Ref*
    * *Ref* and *Pattern* indicate that the associated property should be used
      to identify the client certificate.
    * *PKCS11Id* is used when representing a certificate in a local store and is
      only valid when describing a local configuration.

* **EAP**
    * (optional if **IKEVersion** is 2, otherwise ignored) - [EAP](#EAP-type)
    * Indicating that EAP authentication should be used with the provided
        parameters.

* **Group**
    * (optional if **IKEVersion** is 1, otherwise ignored) - **string**
    * Group name used for machine authentication.

* **IKEVersion**
    * (required) - **integer**
    * Version of IKE protocol to use.

* **PSK**
    * (optional if **AuthenticationType** is *PSK*, otherwise ignored)
      - **string**
    * Pre-Shared Key. If not specified, the user is prompted when connecting.
      If the value is saved but not known, this may be set to an empty value,
      indicating that the UI does not need to provide it.

* **SaveCredentials**
    * (optional if **AuthenticationType**
          is *PSK*, otherwise ignored, defaults to *false*) - **boolean**
    * If *false*, require user to enter credentials
        (PSK) each time they connect.

* **ServerCARefs**
    * (optional if **AuthenticationType** is *Cert*, otherwise rejected)
      - **array of string**
    * Non-empty list of references to CA certificates in **Certificates** to be
      used for verifying the host's certificate chain. At least one of the CA
      certificates must match. If this field is set, **ServerCARef** must be
      unset.

* **ServerCARef**
    * (optional if **AuthenticationType** is *Cert*, otherwise rejected) -
      **string**
    * DEPRECATED, use **ServerCARefs** instead.<br/>
      Reference to a CA certificate in **Certificates**. Certificate authority
      to use for verifying connection. If this field is set, **ServerCARefs**
      must be unset.

* **XAUTH**
    * (optional if **IKEVersion** is 1, otherwise ignored) -
      [XAUTH](#XAUTH-type)
    * Describing XAUTH credentials. XAUTH is not used if this object is not
      present.

---
  * If **AuthenticationType** is set to *Cert*, *ServerCARefs* or *ServerCARef*
    must be set.
  * At most one of **ServerCARefs** and **ServerCARef** can be set
---

### L2TP type

* **LcpEchoDisabled**
    * (optional, defaults to *false*) - **boolean**
    * Disable L2TP connection monitoring via PPP LCP frames.  This
        allows the VPN client to work around server implementations
        that do not support the LCP echo feature.

* **Password**
    * (optional) - **string**
    * User authentication password. If not specified, user is prompted at time
        of connection.

* **SaveCredentials**
    * (optional, defaults to *false*) - **boolean**
    * If *false*, require user to enter credentials
        each time they connect.

* **Username**
    * (optional) - **string**
    * User identity. This value is subject to string expansions. If not
        specified, user is prompted at time of connection.

### XAUTH type

* **Password**
    * (optional) - **string**
    * XAUTH password. If not specified, user is prompted at time of
        connection.

* **SaveCredentials**
    * (optional, defaults to *false*) - **boolean**
    * If *false*, require user to enter credentials
        each time they connect.

* **Username**
    * (optional) - **string**
    * XAUTH user name. This value is subject to string expansions. If not
        specified, user is prompted at time of connection.

## IPsec IKE v1 VPN connections

**VPN.Type** must
be *IPsec*, **IKEVersion**
must be 1. Do not use this for L2TP over IPsec. This may be used for
machine-authentication-only IKEv1 or for IKEv1 with XAUTH. See
the [IPsec](#IPsec-type) type described below.

## IPsec IKE v2 VPN connections

**VPN.Type** must
be *IPsec*, **IKEVersion**
must be 2. This may be used with EAP-based user authentication.

## L2TP over IPsec VPN connections

There are two major configurations L2TP over IPsec which depend on how IPsec
is authenticated. In either case **Type** must be *L2TP-IPsec*.
They are described below.

L2TP over IPsec with pre-shared key:

* The field **IPsec** must be present and have the following settings:
    * **IKEVersion** must be 1.
    * **AuthenticationType** must be PSK.
    * **XAUTH** must not be set.

* The field **L2TP** must be present.


## OpenVPN connections and types

**VPN.Type** must be *OpenVPN*.

### OpenVPN type

* **Auth**
    * (optional, defaults to *SHA1*) - **string**

* **AuthRetry**
    * (optional, defaults to *none*) - **string**
    * Allowed values are:
        * *none* = Fail with error on retry
        * *nointeract* = retry without asking for authentication
        * *interact* = ask again for authentication each time
    * Controls how OpenVPN responds to username/password verification failure.

* **AuthNoCache**
    * (optional, defaults to *false*) - **boolean**
    * Disable caching of credentials in memory.

* **Cipher**
    * (optional, defaults to *BF-CBC*) - **string**
    * Cipher to use.

* **ClientCertPKCS11Id**
    * (required if **ClientCertType** is *PKCS11Id*, otherwise ignored) -
    * PKCS#11 identifier in the format slot:key_id.

* **ClientCertPattern**
    * (required if **ClientCertType** is *Pattern*, otherwise ignored) -
      [CertificatePattern](#CertificatePattern-type)
    * Pattern to use to find the client certificate.

* **ClientCertRef**
    * (required if **ClientCertType** is *Ref*, otherwise ignored) - **string**
    * Reference to client certificate stored in certificate section.

* **ClientCertType**
    * (required) - **string**
    * Allowed values are
      * *PKCS11Id*
      * *Pattern*
      * *Ref*
      * *None*
    * *Ref* and *Pattern* indicate that the associated property should be used
      to identify the client certificate.
    * *PKCS11Id* is used when representing a certificate in a local store and is
      only valid when describing a local configuration.
    * *None* indicates that the server is configured to not require client
      certificates.

* **CompLZO**
    * (optional, defaults to *adaptive*) - **string**
    * Decides to fast LZO compression with *true*
      and *false* as other values.

* **CompNoAdapt**
    * (optional, defaults to *false*) - **boolean**
    * Disables adaptive compression.

* **ExtraHosts**
    * (optional) - **array of string**
    * List of hosts to try in order if client is unable to connect to the
    * primary host.

* **IgnoreDefaultRoute**
    * (optional, defaults to *false*) - **boolean**
    * Omits a default route to the VPN gateway while the connection is active.
      By default, the client creates a default route to the gateway address
      advertised by the VPN server.  Setting this value to
      *true* will allow split tunnelling for
      configurations where the VPN server omits explicit default routes.
      This is roughly equivalent to omitting "redirect-gateway" OpenVPN client
      configuration option.  If the server pushes a "redirect-gateway"
      configuration flag to the client, this option is ignored.

* **KeyDirection**
    * (optional) - **string**
    * Passed as --key-direction.

* **NsCertType**
    * (optional) - **string**
    * If set, checks peer certificate type. Should only be set
      to *server* if set.

* **OTP**
    * (optional if **UserAuthenticationType** is *OTP*, *PasswordAndOTP* or
      unset, otherwise ignored, defaults to empty string) - **string**
    * If **UserAuthenticationType** is
      *OTP* or *PasswordAndOTP*
      and this field is not set, the user will be asked for an OTP.
      The OTP is never persisted and must be provided on every connection
      attempt.

* **Password**
    * (optional if **UserAuthenticationType** is *Password*, *PasswordAndOTP*
      or unset, otherwise ignored, defaults to empty string) - **string**
    * If **UserAuthenticationType** is
      *Password* or
      *PasswordAndOTP* and this field is not set, the user
      will be asked for a password.
      If **SaveCredentials** is
      *true*, the password is persisted for future
      connection attempts. Otherwise it is not persisted but might still be
      reused for consecutive connection attempts (opposed to an OTP, which will
      never be reused).

* **Port**
    * (optional, defaults to *1194*) - **integer**
    * Port for connecting to server.

* **Proto**
    * (optional, defaults to *udp*) - **string**
    * Protocol for communicating with server.

* **PushPeerInfo**
    * (optional, defaults to *false*) - **boolean**

* **RemoteCertEKU**
    * (optional) - **string**
    * Require that the peer certificate was signed with this explicit extended
      key usage in oid notation.

* **RemoteCertKU**
    * (optional, defaults to []) - **array of string**
    * Require the given array of key usage numbers. These are strings that are
      hex encoded numbers.

* **RemoteCertTLS**
    * (optional, defaults to *server*) - **string**
    * Allowed values are:
        * *none*
        * *server*
    * Require peer certificate signing based on RFC3280 TLS rules.

* **RenegSec**
    * (optional, defaults to *3600*) - **integer**
    * Renegotiate data channel key after this number of seconds.

* **SaveCredentials**
    * (optional, defaults to *false*) - **boolean**
    * If *false*, require user to enter credentials
      each time they connect.

* **ServerCAPEMs**
    * (optional) - **array of string**
    * Non-empty list of CA certificates in PEM format, If this field is set,
      **ServerCARef** and **ServerCARefs** must be unset.

* **ServerCARefs**
    * (optional) - **array of string**
    * Non-empty list of references to CA certificates in **Certificates** to be
      used for verifying the host's certificate chain. At least one of the CA
      certificates must match. See also OpenVPN's command line option "--ca".
      If this field is set, **ServerCARef** must be unset.

* **ServerCARef**
    * (optional) - **string**
    * DEPRECATED, use **ServerCARefs** instead.<br/>
      Reference to a CA certificate in **Certificates**. Certificate authority
      to use for verifying connection. If this field is set, **ServerCARefs**
      must be unset.

* **ServerCertRef**
    * (optional) - **string**
    * Reference to a certificate. Peer's signed certificate.

* **ServerPollTimeout**
    * (optional) - **integer**
    * Spend no more than this number of seconds before trying the next server.

* **Shaper**
    * (optional) - **integer**
    * If not specified no bandwidth limiting, otherwise limit bandwidth of
      outgoing tunnel data to this number of bytes per second.

* **StaticChallenge**
    * (optional) - **string**
    * String is used in static challenge response. Note that echoing is always
      done.

* **TLSAuthContents**
    * (optional) - **string**
    * If not set, tls auth is not used. If set, this is the TLS Auth key
      contents (usually starts with "-----BEGIN OpenVPN Static Key..."

* **TLSRemote**
    * (optional) - **string**
    * If set, only allow connections to server hosts with X509 name or common
      name equal to this string.

* **TLSVersionMin**
    * (optional) - **string**
    * If set, specifies the minimum TLS protocol version used by OpenVPN.

* **UserAuthenticationType**
    * (optional, defaults to *None*) - **string**
    * Allowed values are:
        * *None*
        * *Password*
        * *PasswordAndOTP*
        * *OTP*
    * Determines the required form of user authentication:
        * *PasswordAndOTP*: This VPN requires a password
        and an OTP (possibly empty). Both will be send to the server in the
        'password' response using the SCRv1 encoding.
        * *Password*: This VPN requires only a password,
        which will be send without modification to the server in the 'password'
        response (no CRv1 or SCRv1 encoding).
        * *OTP*: This VPN requires only an OTP, which
        will be send without modification to the server in the 'password'
        response (no CRv1 or SCRv1 encoding).
        * *None*: Neither password nor OTP are required.
        No password request from the server is expected.
      If not set, the user can provide a password and an OTP (both not
      mandatory) and the network manager will send both in the SCRv1 encoding,
      when the server sends a static-challenge. If the server does not send a
      static-challenge, the client will reply with only the password (without
      any encoding). This behavior is deprecated and new configurations should
      explicitly set one of the above values.

      See the fields **Password** and
      **OTP** for configuring the password and OTP.

* **Username**
    * (optional) - **string**
    * OpenVPN user name. This value is subject to string expansions. If not
      specified, user is prompted at time of connection.

* **Verb**
    * (optional) - **string**
    * Verbosity level, defaults to OpenVpn's default if not specified.

* **VerifyHash**
    * (optional) - **string**
    * If set, this value is passed as the "--verify-hash" argument to OpenVPN,
      which specifies the SHA1 fingerprint for the level-1 certificate.

* **VerifyX509**
    * (optional) - [VerifyX509](#VerifyX509-type)
    * If set, the "--verify-x509-name" argument is passed to OpenVPN with the
      values of this object and only connections will be accepted if a host's
      X.509 name is equal to the given name.

---
  * At most one of **ServerCARefs** and **ServerCARef** can be set.
---

### VerifyX509 type

* **Name**
    * (required) - **string**
    * The name that the host's X.509 name is compared to. Which host name is
      compared depends on the value of **Type**.

* **Type**
    * (optional) - **string**
    * Determines which of the host's X.509 names will be verified.
    * Allowed values are:
        * *name*
        * *name-prefix*
        * *subject*
      See OpenVPN's documentation for "--verify-x509-name" for the meaning of
      each value. Defaults to OpenVPN's default if not specified.

## Third-party VPN provider based connections and types

**VPN.Type** must be *ThirdPartyVPN*.

### ThirdPartyVPN type

* **ExtensionID**
    * (required) - **string**
    * The extension ID of the third-party VPN provider used by this network.
* **ProviderName**
    * (optional, read-only) - **string**
    * The name of the third-party VPN provider used by this network.


## Client certificate patterns

In order to allow clients to securely key their private keys and request
certificates through PKCS#10 format or through a web flow, we provide
alternative CertificatePattern types. The

### CertificatePattern type

* **IssuerCARef**
    * (optional) - **array of string**
    * Array of references to certificates. At least one must have signed the
      client certificate.

* **Issuer**
    * (optional) - [IssuerSubjectPattern](#IssuerSubjectPattern-type)
    * Pattern to match the issuer X.509 settings against. If not specified, the
      only checks done will be a signature check against
      the **IssuerCARef** field. Issuer of the
      certificate must match this field exactly to match the pattern.

* **Subject**
    * (optional) - [IssuerSubjectPattern](#IssuerSubjectPattern-type)
    * Pattern to match the subject X.509 settings against. If not specified, the
      subject settings are not checked and any certificate matches. Subject of
      the certificate must match this field exactly to match the pattern.

* **EnrollmentURI**
    * (optional) - **array of string**
    * If no certificate matches this CertificatePattern, the first URI from this
      array with a recognized scheme is navigated to, with the intention this
      informs the user how to either get the certificate or gets the certificate
      for the user. For instance, the array may be [
      "chrome-extension://asakgksjssjwwkeielsjs/fetch-client-cert.html",
      "http://intra/connecting-to-wireless.html" ] so that for Chrome browsers a
      Chrome app or extension is shown to the user, but for other browsers, a
      web URL is shown.

### IssuerSubjectPattern type

* **CommonName**
    * (optional) - **string**
    * Certificate subject's commonName must match this string if present.

* **Locality**
    * (optional) - **string**
    * Certificate subject's location must match this string if present.

* **Organization**
    * (optional) - **string**
    * At least one of certificate subject's organizations must match this string
      if present.

* **OrganizationalUnit**
    * (optional) - **string**
    * At least one of certificate subject's organizational units must match this
      string if present.

---
  * One field in **Subject**, **Issuer**, or **IssuerCARef**
    must be given for a [CertificatePattern](#CertificatePattern-type) typed
    field to be valid.
  * For a certificate to be considered matching, it must match all
    the fields in the certificate pattern. If multiple certificates match, the
    certificate with the latest issue date that is still in the past, and
    hence valid, will be used.
  * If **EnrollmentURI** is not given and no match is
    found to this pattern, the importing tool may show an error to the user.
---

## Proxy settings

Every network can be configured to use a proxy.

### ProxySettings type

* **Type**
    * (required) - **string**
    * Allowed values are:
        * *Direct*,
        * *Manual*
        * *PAC*
        * *WPAD*
    * *PAC* indicates Proxy Auto-Configuration.
      *WPAD* indicates Web Proxy Autodiscovery.

* **Manual**
    * (required if **Type** is *Manual*, otherwise ignored) -
      [ManualProxySettings](#ManualProxySettings-type)
    * Manual proxy settings.

* **ExcludeDomains**
    * (optional if **Type** is *Manual*, otherwise ignored) -
      **array of string**
    * Domains and hosts for which to exclude proxy settings.

* **PAC**
    * (required if **Type** is *PAC*, otherwise ignored) - **string**
    * URL of proxy auto-config file.

### ManualProxySettings type

* **HTTPProxy**
    * (optional) - [ProxyLocation](#ProxyLocation-type)
    * settings for HTTP proxy.

* **SecureHTTPProxy**
    * (optional) - [ProxyLocation](#ProxyLocation-type)
    * settings for secure HTTP proxy.

* **FTPProxy**
    * (optional) - [ProxyLocation](#ProxyLocation-type)
    * settings for FTP proxy

* **SOCKS**
    * (optional) - [ProxyLocation](#ProxyLocation-type)
    * settings for SOCKS proxy.

### ProxyLocation type

* **Host**
    * (required) - **string**
    * Host (or IP address) to use for proxy

* **Port**
    * (required) - **integer**
    * Port to use for proxy

## EAP configurations

For networks with 802.1X authentication, an [EAP](#EAP-type)
type exists to configure the authentication.

### EAP type

* **AnonymousIdentity**
    * (optional if **Outer** is
        *PEAP* or *EAP-TTLS*, otherwise ignored) - **string**
    * For tunnelling protocols only, this indicates the identity of the user
      presented to the outer protocol. This value is subject to string
      expansions. If not specified, use empty string.

* **ClientCertPKCS11Id**
    * (required if **ClientCertType** is *PKCS11Id*, otherwise ignored) -
    * PKCS#11 identifier in the format slot:key_id.

* **ClientCertPattern**
    * (required if **ClientCertType** is *Pattern*, otherwise ignored) -
      [CertificatePattern](#CertificatePattern-type)
    * Pattern to use to find the client certificate.

* **ClientCertRef**
    * (required if **ClientCertType** is *Ref*, otherwise ignored) - **string**
    * Reference to client certificate stored in certificate section.

* **ClientCertType**
    * (optional) - **string**
    * Allowed values are:
        * *PKCS11Id*
        * *Pattern*
        * *Ref*
        * *None*
    * *Ref* and *Pattern* indicate that the associated property should be used
      to identify the client certificate.
    * *PKCS11Id* is used when representing a certificate in a local store and is
      only valid when describing a local configuration.
    * *None* indicates that the server is configured to not require client
      certificates.

* **Identity**
    * (optional) - **string**
    * Identity of user. For tunneling outer protocols
      (*PEAP*, *EAP-TTLS*, and
      *EAP-FAST*), this is used to authenticate inside
      the tunnel, and **AnonymousIdentity** is used for
      the EAP identity outside the tunnel. For non-tunneling outer protocols,
      this is used for the EAP identity. This value is subject to string
      expansions.

* **Inner**
    * (optional if **Outer** is *EAP-FAST*, *EAP-TTLS* or *PEAP*, otherwise
        ignored, defaults to *Automatic*) - **string**
    * Allowed values are:
        * *Automatic*
        * *MD5*
        * *MSCHAP*
        * *MSCHAPv2*
        * *PAP*
        * *CHAP*
        * *GTC*
    * For tunneling outer protocols.

* **Outer**
    * (required) - **string**
    * Allowed values are:
        * *LEAP*
        * *EAP-AKA*
        * *EAP-FAST*
        * *EAP-TLS*
        * *EAP-TTLS*
        * *EAP-SIM*
        * *PEAP*

* **Password**
    * (optional) - **string**
    * Password of user. If not specified, defaults to prompting the user.

* **SaveCredentials**
    * (optional, defaults to *false*) - **boolean**
    * If *false*, require user to enter credentials each time they connect.
      Specifying **Identity** and/or **Password** when **SaveCredentials**
      is *false* is not allowed.

* **ServerCAPEMs**
    * (optional) - **array of string**
    * Non-empty list of CA certificates in PEM format, If this field is set,
      **ServerCARef** and **ServerCARefs** must be unset.

* **ServerCARefs**
    * (optional) - **array of string**
    * Non-empty list of references to CA certificates in **Certificates** to be
      used for verifying the host's certificate chain. At least one of the CA
      certificates must match. If this field is set, **ServerCARef** must be
      unset. If neither **ServerCARefs** nor **ServerCARef** is set, the client
      does not check that the server certificate is signed by a specific CA.
      A verification using the system's CA certificates may still apply.
      See **UseSystemCAs** for this.

* **ServerCARef**
    * (optional) - **string**
    * DEPRECATED, use **ServerCARefs** instead.<br/>
      Reference to a CA certificate in **Certificates**.
    * If this field is set, **ServerCARefs** must be unset.
      If neither **ServerCARefs** nor **ServerCARef** is set, the client does
      not check that the server certificate is signed by a specific CA.
      A verification using the system's CA certificates may still apply.
      See **UseSystemCAs** for this.

* **SubjectMatch**
    * (optional) - **string**
    * WiFi only. A substring which a remote RADIUS service certificate subject
      name must contain in order to connect.

* **TLSVersionMax**
    * (optional) - **string**
    * Sets the maximum TLS protocol version used by the OS for EAP.
      This is only needed when connecting to an AP with a buggy TLS
      implementation, as the protocol will normally auto-negotiate.
    * Allowed values are:
        * *1.0*
        * *1.1*
        * *1.2*

* **UseSystemCAs**
    * (optional, defaults to *true*) - **boolean**
    * Required server certificate to be signed by "system default certificate
      authorities". If both **ServerCARefs** (or **ServerCARef**)
      and **UseSystemCAs** are supplied, a server
      certificate will be allowed if it either has a chain of trust to a system
      CA or to one of the given CA certificates. If **UseSystemCAs**
      is *false*, and no **ServerCARef** is set, the certificate
      must be a self signed certificate, and no CA signature is required.

* **UseProactiveKeyCaching**
    * (optional, defaults to *false*) - **boolean**
    * Indicates whether Proactive Key Caching (also known as Opportunistic
      Key Caching) should be used on a per-service basis.

---
  * At most one of **ServerCARefs** and **ServerCARef**
    can be set.
---

## Cellular Networks

For Cellular connections, **Type** must be set to *Cellular* and the
field **Cellular** must be set to an object of type [Cellular](#Cellular-type).

Currently only used for representing an existing configuration;
ONC configuration of of **Cellular** networks is not yet supported.

### Cellular type

* **AutoConnect**
    * (optional, defaults to *false*) - **boolean**
    * Indicating that the network should be connected to automatically when
      possible. Note, that disabled **AllowRoaming**
      takes precedence over autoconnect.

* **APN**
    * (optional) - [APN](#APN-type)
    * Currently active  [APN](#APN-type) object to be used with a
      GSM carrier for making data connections.

* **APNList**
    * (optional) - [array of APN](#APN-type)
    * List of available APN configurations.

* **ActivationType**
    * (optional) - **string**
    * Activation type.

* **ActivationState**
    * (optional, read-only) - **string**
    * Carrier account activation state.
    * Allowed values are:
        * *Activated*
        * *Activating*
        * *NotActivated*
        * *PartiallyActivated*

* **AllowRoaming**
    * (optional) - **boolean**
    * Whether cellular data connections are allowed when the device is roaming.

* **Carrier**
    * (optional, read-only) - **string**
    * The name of the carrier for which the device is configured.

* **ESN**
    * (optional, read-only) - **string**
    * The Electronic Serial Number of the cellular modem.

* **Family**
    * (optional, read-only) - **string**
    * Technology family.
    * Allowed values are:
        * *CDMA*
        ** GSM*

* **FirmwareRevision**
    * (optional, read-only) - **string**
    * The revision of firmware that is loaded in the modem.

* **FoundNetworks**
    * (optional, read-only, provided only if **Family** is *GSM*) -
      [array of FoundNetwork](#FoundNetwork-type)
    * The list of cellular netwoks found in the most recent scan operation.

* **HardwareRevision**
    * (optional, read-only) - **string**
    * The hardware revision of the cellular modem.

* **HomeProvider**
    * (optional, read-only) - [CellularProvider](#CellularProvider-type)
    * Description of the operator that issued the SIM card currently installed
      in the modem.

* **ICCID**
    * (optional, read-only, provided only if **Family** is *GSM*
        or **NetworkTechnology**
        is *LTE*) - **string**
    * For GSM / LTE modems, the Integrated Circuit Card Identifer of the SIM
      card installed in the device.

* **IMEI**
    * (optional, read-only) - **string**
    * The International Mobile Equipment Identity of the cellular modem.

* **IMSI**
    * (optional, read-only, provided only if **Family** is *GSM*) - **string**
    * For GSM modems, the International Mobile Subscriber Identity of the SIM
      card installed in the device.

* **LastGoodAPN**
    * (optional, read-only) - [APN](#APN-type)
    * The APN information used in the last successful connection attempt.

* **Manufacturer**
    * (optional, read-only) - **string**
    * The manufacturer of the cellular modem.

* **MDN**
    * (optional) - **string**
    * The Mobile Directory Number (i.e., phone number) of the device.

* **MEID**
    * (optional, read-only, provided only if **Family** is *CDMA*) - **string**
    * For CDMA modems, the Mobile Equipment Identifer of the cellular modem.

* **MIN**
    * (optional, read-only) - **string**
    * The Mobile Identification Number of the device.

* **ModelID**
    * (optional, read-only) - **string**
    * The hardware model of the cellular modem.

* **NetworkTechnology**
    * (optional, read-only) - **string**
    * If the modem is registered on a network, then this is set to the
      network technology currently in use.
    * Allowed values are:
        * *CDMA1XRTT*
        * *EDGE*
        * *EVDO*
        * *GPRS*
        * *GSM*
        * *HSPA*
        * *HSPAPlus*
        * *LTE*
        * *LTEAdvanced*
        * *UMTS*

* **PaymentPortal**
    * (optional, read-only) - [PaymentPortal](#PaymentPortal-type)
    * Properties describing the online payment portal (OLP) at which a user can
      sign up for or modify a mobile data plan.

* **RoamingState**
    * (optional, read-only) - **string**
    * The roaming status of the cellular modem on the current network.
    * Allowed values are:
        * *Home*,
        * *Roaming*
        * *Required* - the provider has no home network

* **Scanning**
    * (optional, read-only) - **boolean**
    * True when a cellular network scan is in progress.

* **ServingOperator**
    * (optional, read-only, provided only if **Family** is *GSM*) -
      [CellularProvider](#CellularProvider-type)
    * Description of the operator on whose network the modem is currently
      registered

* **SignalStrength**
    * (optional, read-only) - **integer**
    * The current signal strength for this network in the range [0, 100],
      provided by the system. If the network is not in range this field will
      be set to '0' or not present.

* **SIMLockStatus**
    * (optional, read-only, provided only if **Family** is *GSM*) -
      [SIMLockStatus](#SIMLockStatus-type)
    * For GSM modems, a dictionary containing two properties describing the
      state of the SIM card lock.

* **SIMPresent**
    * (optional, read-only, provided only if **Family** is *GSM*
        or **NetworkTechnology**
        is *LTE*) - **boolean**
    * For GSM or LTE modems, indicates whether a SIM card is present or not.

* **SupportNetworkScan**
    * (optional, read-only) - **boolean**
    * True if the cellular network supports scanning.


### APN type

* **AccessPointName**
    * (required) - **string**
    * The access point name used when making connections.

* **Name**
    * (optional) - **string**
    * Description of the APN.

* **LocalizedName**
    * (optional) - **string**
    * Localized description of the APN.

* **Username**
    * (optional) - **string**
    * Username for making connections if required.

* **Password**
    * (optional) - **string**
    * Password for making connections if required.

* **Authentication**
    * (optional) - **string**
    * Type of authentication protocol for sending username and password.

* **Language**
    * (optional, rquired if **LocalizedName** is provided) - **string**
      Two letter language code for Localizedname if provided.

### FoundNetwork type

* **Status**
    * (required) - **string**
    * The availability of the network.

* **NetworkId**
    * (required) - **string**
    * The network id in the form MCC/MNC (without the '/').

* **Technology**
    * (required) - **string**
    * Access technology used by the network,
      e.g. "GSM", "UMTS", "EDGE", "HSPA", etc.

* **ShortName**
    * (optional) - **string**
    * Short-format name of the network operator.

* **LongName**
    * (optional) - **string**
    * Long-format name of the network operator.

### PaymentPortal type

* **Method**
    * (required) - **string**
    * The HTTP method to use, "GET" or "POST"

* **PostData**
    * (required if **Method** is *POST*, otherwise ignored) - **string**
    * The postdata to send.

* **Url**
    * (required) - **string**
    * The URL for the portal.

### CellularProvider type

* **Name**
    * (required) - **string**
    * The operator name.

* **Code**
    * (required) - **string**
    * The network id in the form MCC/MNC (without the '/').

* **Country**
    * (optional) - **string**
    * The two-letter country code.

### SIMLockStatus type

* **LockType**
    * (required) - **string**
    * Specifies the type of lock in effect, or an empty string if unlocked.
    * Allowed values are:
        * *sim-pin*,
        * *sim-puk*

* **LockEnabled**
    * (required) - **boolean**
    * Indicates whether SIM locking is enabled

* **RetriesLeft**
    * (optional) - **integer**
    * If **LockType** is empty
      or *sim-pin*, then this property represents
      the number of attempts remaining to supply a correct PIN before the
      PIN becomes blocked, at which point a PUK provided by the carrier would
      be necessary to unlock the SIM (and **LockType**
      changes to *sim-puk*).


## Tether Networks

For Tether connections, **Type** must be set to *Tether* and the
field **Tether** must be set to an object of type [Tether](#Tether-type).

Used for representing a tether hotspot provided by an external device, e.g.
a phone.

### Tether type

* **BatteryPercentage**
    * (optional, read-only) - **integer**
    * The battery percentage of the device providing the tether hotspot in the
      range [0, 100].

* **Carrier**
    * (optional, read-only) - **string**
    * The name of the cellular carrier when the hotspot is provided by a
      cellular connection.

* **HasConnectedToHost**
    * (read-only) - **boolean**
    * If *true*, the current device has already connected to a Tether network
      created by the same external device which is providing this Tether
      network.

* **SignalStrength**
    * (optional, read-only) - **integer**
    * The current signal strength for the hotspot's connection in the range
      [0, 100]. Note that this value refers to the strength of the signal
      between the external device and its data provider, not the strength of the
      signal between the current device and the external device.


## Bluetooth / WiFi Direct Networks

This format will eventually also cover configuration of Bluetooth and WiFi
Direct network technologies, however they are currently not supported.


## Certificates

Certificate data is stored in a separate section. Each certificate may be
referenced from within the NetworkConfigurations array using a certificate
reference. A certificate reference is its GUID.

The top-level field **Certificates** is an array of
objects of [Certificate](#Certificate-type) type.

### Certificate type

* **GUID**
    * (required) - **string**
    * A unique identifier for this certificate. Must be a non-empty string.

* **PKCS12**
    * (required if **Type** is
        *Client*, otherwise ignored) - **string**
    * For certificates with
      private keys, this is the base64 encoding of the a PKCS#12 file.

* **Remove**
    * (optional, defaults to *false*) - **boolean**
    * If *true*, remove this certificate (only GUID
      should be set).

* **Scope**
    * (optional, default Scope if missing) - [Scope](#Scope-type)
    * If this is given, it specifies the scope in which the certificate should
      be applied.

* **TrustBits**
    * (optional if **Type**
        is *Server*
        or *Authority*, otherwise ignored, defaults to []) - **array of string**
    * An array of trust flags. Clients should ignore unknown flags. For
      backwards compatibility, each flag should only increase the trust and
      never restrict. The trust flag *Web* implies that
      the certificate is to be trusted for HTTPS SSL identification. A typical
      web certificate authority would have **Type** set
      to *Authority* and **TrustBits** set to `["Web"]`

* **Type**
    * (required if **Remove** is *false*, otherwise ignored) - **string**
    * Allowed values are:
        * *Client*
        * *Server*
        * *Authority*
    * *Client* indicates the certificate is for
      identifying the user or device over HTTPS or for
      VPN/802.1X. *Server* indicates the certificate
      identifies an HTTPS or VPN/802.1X peer.
      *Authority* indicates the certificate is a
      certificate authority and any certificates it issues should be
      trusted. Note that if **Type** disagrees with the
      x509 v3 basic constraints or key usage attributes, the
      **Type** field should be honored.

* **X509**
    * (required if **Type** is
        *Server* or *Authority*, otherwise ignored) - **string**
    * For certificate
      without private keys, this is the X509 certificate in PEM format.

    The passphrase of the PKCS#12 encoding must be empty. Encryption of key data
    should be handled at the level of the entire file, or the transport of the
    file.

    If a global-scoped network connection refers to a user-scoped certificate,
    results are undefined, so this configuration should be prohibited by the
    configuration editor.

### Scope type
* **Id**
    * (required if **Type** is *Extension*, otherwise ignored) - **string**
    * If *Type* is *Extension*, this is the ID of the chrome extension for which
      the certificate should be applied.
* **Type**
    * (required) - **string**
    * Allowed values are:
        * *Extension*
        * *Default*
    * *Extension* indicates that the certificate should only be applied in the
      scope of a chrome extension.
      *Default* indicates that the scope the certificate applies in should not
      be restricted.


## Encrypted Configuration

We assume that when this format is imported as part of policy that
file-level encryption will not be necessary because the policy transport is
already encrypted, but when it is imported as a standalone file, it is
desirable to encrypt it. Since this file has private information (user
names) and secrets (passphrases and private keys) in it, and we want it to
be usable as a manual way to distribute network configuration, we must
support encryption.

For this standalone export, the entire file will be encrypted in a symmetric
fashion with a passphrase stretched using salted PBKDF2 using at least 20000
iterations, and encrypted using an AES-256 CBC mode cipher with an SHA-1
HMAC on the ciphertext.

An encrypted ONC file's top level object will have the
[EncryptedConfiguration](#EncryptedConfiguration-type) type.

### EncryptedConfiguration type

* **Cipher**
    * (required) - **string**
    * The type of cipher used. Currently only *AES256*
      is supported.

* **Ciphertext**
    * (required) - **string**
    * The raw ciphertext of the encrypted ONC file, base64 encoded.

* **HMAC**
    * (required) - **string**
    * The HMAC for the ciphertext, base64 encoded.

* **HMACMethod**
    * (required) - **string**
    * The method used to compute the Hash-based Message Authentication Code
      (HMAC). Currently only *SHA1* is supported.

* **Salt**
    * (required) - **string**
    * The salt value used during key stretching.

* **Stretch**
    * (required) - **string**
    * The key stretching algorithm used. Currently
      only *PBKDF2* is supported.

* **Iterations**
    * (required) - **integer**
    * The number of iterations to use during key stretching.

* **IV**
    * (required) - **string**
    * The initial vector (IV) used for Cyclic Block Cipher (CBC) mode, base64
      encoded.

* **Type**
    * (required) - **string**
    * The type of the ONC file, which must be set
      to *EncryptedConfiguration*.

---
  * When decrypted, the ciphertext must contain a JSON object of
    type [UnencryptedConfiguration](#UnencryptedConfiguration-type).
---

## String Expansions

The values of some fields, such
as **WiFi.EAP.Identity**
and **VPN.*.Username**, are subject to string
expansions. These allow one ONC to have basic user-specific variations.

### The expansions are:

* Placeholders that will only be replaced in user-specific ONC:
    * ${LOGIN\_ID} - expands to the email address of the user, but before
      the '@'.
    * ${LOGIN\_EMAIL} - expands to the email address of the user.

* Placeholders that will only be replaced in device-wide ONC:
    * ${DEVICE\_SERIAL\_NUMBER} - expands to the serial number of the device.
    * ${DEVICE\_ASSET\_ID} - expands to the administrator-set asset ID of the
      device.

* Placeholders that will only be replaced when a client certificate has been
  matched by a [CertificatePattern](#CertificatePattern-type):
    * ${CERT\_SAN\_EMAIL} - expands to the first RFC822 SubjectAlternativeName
      extracted from the client certificate.
    * ${CERT\_SAN\_UPN} - expands to the first OtherName SubjectAlternativeName
      with OID 1.3.6.1.4.1.311.20.2.3 (UserPrincipalName) extracted from the
      client certificate.
    * ${CERT\_SUBJECT\_COMMON\_NAME} - expands to the ASCII value of the Subject
      CommonName extracted from the client certificate.

### The following SED would properly handle resolution.

* s/\$\{LOGIN\_ID\}/bobquail$1/g

* s/\$\{LOGIN\_EMAIL\}/bobquail@example.com$1/g

### Example expansions, assuming the user was bobquail@example.com:

* "${LOGIN\_ID}" -> "bobquail"

* "${LOGIN\_ID}@corp.example.com" -> "bobquail@corp.example.com"

* "${LOGIN\_EMAIL}" -> "bobquail@example.com"

* "${LOGIN\_ID}X" -> "bobquailX"

* "${LOGIN\_IDX}" -> "${LOGIN\_IDX}"

* "X${LOGIN\_ID}" -> "Xbobquail"


## String Substitutions
The value of **WiFi.EAP.Password** is subject to string substitution. These
differ from the **String Expansions** section above in that an exact match of
the substitution variable is required in order to substitute the real value.

### Example expansions, assuming the user password was *helloworld*:

* "${PASSWORD}" -> "helloworld"

* "${PASSWORD}foo" -> "${PASSWORD}foo"

## Recommended Values
When a policy is providing ONC configurations, the assumption is that all values
are mandatory and immutable. To specify values that can be overridden by a user
(e.g. proxy or username), use the **Recommended** property.

* **Recommended**
  * (optional) - **array of string**
  * The field(s) with the names in the strings in this array are to be treated
    as recommended settings. Any fields not mentioned in this array remain
    mandatory. This also means that fields that are not mentioned in the array
    and also not mentioned in the objects are mandatory and have the default
    value of the field. If not present, all fields are mandatory. Fields that
    are objects or arrays of objects included in Recommended will be ignored. In
    those cases, the nested objects should have their own Recommended fields. A
    special case is if the string "." is included in the list. When this is
    present, it means that the entire certificate or network can be forgotten or
    deleted by the user. Including the "." has no implications on the rest of
    the settings. For instance, a network may have Recommended set to [ "." ],
    in which case its settings may not be changed by the user, but the whole
    network can be forgotten by the user.  The "." is valid in a Certificate
    object and the NetworkConfiguration object, it is ignored elsewhere.


## Detection

This format should be sent in files ending in the .onc extension. When
transmitted with a MIME type, the MIME type should be
application/x-onc. These two methods make detection of data to be handled in
this format, especially when encryption is used and the payload itself is
not detectable.


## Alternatives considered

For the overall format, we considered XML, ASN.1, and protobufs. JSON and
ASN.1 seem more widely known than protobufs. Since administrators are
likely to want to tweak settings that will not exist in common UIs, we
should provide a format that is well known and human modifiable. ASN.1 is
not human modifiable. Protobufs formats are known by open source developers
but seem less likely to be known by administrators. JSON serialization
seems to have good support across languages.

We considered sending the exact connection manager configuration format of
an open source connection manager like connman. There are a few issues
here, for instance, referencing certificates by identifiers not tied to a
particular PKCS#11 token, and tying to one OS's connection manager.

## Examples

### GlobalNetworkConfiguration Example

In this example, we only allow managed networks to auto connect and
disallow any other networks if a managed network is available. We also blacklist
the "Guest" network (hex("Guest")=4775657374) and disable Cellular services.
```
{
  "Type": "UnencryptedConfiguration",
  "GlobalNetworkConfiguration": {
    "AllowOnlyPolicyNetworksToAutoconnect": true,
    “AllowOnlyPolicyNetworksToConnect”: false,
    “AllowOnlyPolicyNetworksToConnectIfAvailable”: true,
    “BlacklistedHexSSIDs”: [“4775657374”],
    "DisableNetworkTypes": ["Cellular"]
  }
}
```

### Simple format example: PEAP/MSCHAPv2 network (per device)

```
{
  "Type": "UnencryptedConfiguration",
  "NetworkConfigurations": [
    {
      "GUID": "{f2c17903-b0e1-8593-b3ca74f977236bd7}",
      "Name": "MySSID",
      "Type": "WiFi",
      "WiFi": {
        "AutoConnect": true,
        "EAP": {
          "Outer": "PEAP",
          "UseSystemCAs": true
        },
        "HiddenSSID": false,
        "SSID": "MySSID",
        "Security": "WPA-EAP"
      }
    }
  ],
  "Certificates": []
}
```

Notice that in this case, we do not provide a username and password - we set
SaveCredentials to *false* so we are prompted every
time. We could have passed in username and password - but such a file should
be encrypted.

### Complex format example: TLS network with client certs (per device)

```
{
  "Type": "UnencryptedConfiguration",
  "NetworkConfigurations": [
    {
      "GUID": "{00f79111-51e0-e6e0-76b3b55450d80a1b}",
      "Name": "MyTTLSNetwork",
      "Type": "WiFi",
      "WiFi": {
        "AutoConnect": false,
        "EAP": {
          "ClientCertPattern": {
            "EnrollmentURI": [
              "http://fetch-my-certificate.com"
            ],
            "IssuerCARef": [
              "{6ed8dce9-64c8-d568-d225d7e467e37828}"
            ]
          },
          "ClientCertType": "Pattern",
          "Outer": "EAP-TLS",
          "ServerCARef": "{6ed8dce9-64c8-d568-d225d7e467e37828}",
          "UseSystemCAs": true
        },
        "HiddenSSID": false,
        "SSID": "MyTTLSNetwork",
        "Security": "WPA-EAP"
      }
    }
  ],
  "Certificates": [
    {
      "GUID": "{6ed8dce9-64c8-d568-d225d7e467e37828}",
      "Type": "Authority",
      "X509": "MIIEpzCCA4+gAwIBAgIJAMueiWq5WEIAMA0GCSqGSIb3DQEBBQUAMIGTMQswCQYDVQQGEwJGUjEPMA0GA1UECBMGUmFkaXVzMRIwEAYDVQQHEwlTb21ld2hlcmUxFTATBgNVBAoTDEV4YW1wbGUgSW5jLjEgMB4GCSqGSIb3DQEJARYRYWRtaW5AZXhhbXBsZS5jb20xJjAkBgNVBAMTHUV4YW1wbGUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5MB4XDTExMDEyODA2MjA0MFoXDTEyMDEyODA2MjA0MFowgZMxCzAJBgNVBAYTAkZSMQ8wDQYDVQQIEwZSYWRpdXMxEjAQBgNVBAcTCVNvbWV3aGVyZTEVMBMGA1UEChMMRXhhbXBsZSBJbmMuMSAwHgYJKoZIhvcNAQkBFhFhZG1pbkBleGFtcGxlLmNvbTEmMCQGA1UEAxMdRXhhbXBsZSBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC9EDplhyrVNJIoy1OsVqvD/K67B5PW2bDKKxGznodrzCu8jHsP1Ne3mgrK20vbzQUUBdmxTCWO6x3a3//r4ZuPOuZd1ViycWjt6mRfRbBzNrHzP7NiyFuXjdlz74beHQQLcHwvZ3qFAWZK37uweiLiDPaMaEQlka2Bztqx4PsogmSdoVPSCxi5Cl1XlJmITA03LlKpO79+0rEPRamWO/DMCwvffn2/UUjJLog4/lYe16HQ6iq/6bjhffm2rLXDFKOGZmBVbLNMCfANRMtdFWHYdBXERoUo2zpM9tduOOUNLy7E7kRKVm/wy38s51ChFPlpORrhimN2j1caar+KAv2tAgMBAAGjgfswgfgwHQYDVR0OBBYEFBTIImiXp+57jjgn2N5wq93GgAAtMIHIBgNVHSMEgcAwgb2AFBTIImiXp+57jjgn2N5wq93GgAAtoYGZpIGWMIGTMQswCQYDVQQGEwJGUjEPMA0GA1UECBMGUmFkaXVzMRIwEAYDVQQHEwlTb21ld2hlcmUxFTATBgNVBAoTDEV4YW1wbGUgSW5jLjEgMB4GCSqGSIb3DQEJARYRYWRtaW5AZXhhbXBsZS5jb20xJjAkBgNVBAMTHUV4YW1wbGUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5ggkAy56JarlYQgAwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOCAQEAnNd0YY7s2YVYPsgEgDS+rBNjcQloTFWgc9Hv4RWBjwcdJdSPIrpBp7LSjC96wH5U4eWpQjlWbOYQ9RBq9Z/RpuAPEjzRV78rIrQrCWQ3lxwywWEb5Th1EVJSN68eNv7Ke5BlZ2l9kfLRKFm5MEBXX9YoHMX0U8I8dPIXfTyevmKOT1PuEta5cQOM6/zH86XWn6WYx3EXkyjpeIbVOw49AqaEY8u70yBmut4MO03zz/pwLjV1BWyIkXhsrtuJyA+ZImvgLK2oAMZtGGFo7b0GW/sWY/P3R6Un3RFy35k6U3kXCDYYhgZEcS36lIqcj5y6vYUUVM732/etCsuOLz6ppw=="
    }
  ]
}
```

In this example, the client certificate is not sent in the ONC format, but
rather we send a certificate authority which we know will have signed the
client certificate that is needed, along with an enrollment URI to navigate
to if the required certificate is not yet available on the client.

### Simple format example: HTTPS Certificate Authority

In this example a new certificate authority is added to be trusted for HTTPS
server authentication.

```
{
  "Type": "UnencryptedConfiguration",
  "NetworkConfigurations": [],
  "Certificates": [
    {
      "GUID": "{f31f2110-9f5f-61a7-a8bd7c00b94237af}",
      "TrustBits": [ "Web" ],
      "Type": "Authority",
      "X509": "MIIEpzCCA4+gAwIBAgIJAMueiWq5WEIAMA0GCSqGSIb3DQEBBQUAMIGTMQswCQYDVQQGEwJGUjEPMA0GA1UECBMGUmFkaXVzMRIwEAYDVQQHEwlTb21ld2hlcmUxFTATBgNVBAoTDEV4YW1wbGUgSW5jLjEgMB4GCSqGSIb3DQEJARYRYWRtaW5AZXhhbXBsZS5jb20xJjAkBgNVBAMTHUV4YW1wbGUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5MB4XDTExMDEyODA2MjA0MFoXDTEyMDEyODA2MjA0MFowgZMxCzAJBgNVBAYTAkZSMQ8wDQYDVQQIEwZSYWRpdXMxEjAQBgNVBAcTCVNvbWV3aGVyZTEVMBMGA1UEChMMRXhhbXBsZSBJbmMuMSAwHgYJKoZIhvcNAQkBFhFhZG1pbkBleGFtcGxlLmNvbTEmMCQGA1UEAxMdRXhhbXBsZSBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC9EDplhyrVNJIoy1OsVqvD/K67B5PW2bDKKxGznodrzCu8jHsP1Ne3mgrK20vbzQUUBdmxTCWO6x3a3//r4ZuPOuZd1ViycWjt6mRfRbBzNrHzP7NiyFuXjdlz74beHQQLcHwvZ3qFAWZK37uweiLiDPaMaEQlka2Bztqx4PsogmSdoVPSCxi5Cl1XlJmITA03LlKpO79+0rEPRamWO/DMCwvffn2/UUjJLog4/lYe16HQ6iq/6bjhffm2rLXDFKOGZmBVbLNMCfANRMtdFWHYdBXERoUo2zpM9tduOOUNLy7E7kRKVm/wy38s51ChFPlpORrhimN2j1caar+KAv2tAgMBAAGjgfswgfgwHQYDVR0OBBYEFBTIImiXp+57jjgn2N5wq93GgAAtMIHIBgNVHSMEgcAwgb2AFBTIImiXp+57jjgn2N5wq93GgAAtoYGZpIGWMIGTMQswCQYDVQQGEwJGUjEPMA0GA1UECBMGUmFkaXVzMRIwEAYDVQQHEwlTb21ld2hlcmUxFTATBgNVBAoTDEV4YW1wbGUgSW5jLjEgMB4GCSqGSIb3DQEJARYRYWRtaW5AZXhhbXBsZS5jb20xJjAkBgNVBAMTHUV4YW1wbGUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5ggkAy56JarlYQgAwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOCAQEAnNd0YY7s2YVYPsgEgDS+rBNjcQloTFWgc9Hv4RWBjwcdJdSPIrpBp7LSjC96wH5U4eWpQjlWbOYQ9RBq9Z/RpuAPEjzRV78rIrQrCWQ3lxwywWEb5Th1EVJSN68eNv7Ke5BlZ2l9kfLRKFm5MEBXX9YoHMX0U8I8dPIXfTyevmKOT1PuEta5cQOM6/zH86XWn6WYx3EXkyjpeIbVOw49AqaEY8u70yBmut4MO03zz/pwLjV1BWyIkXhsrtuJyA+ZImvgLK2oAMZtGGFo7b0GW/sWY/P3R6Un3RFy35k6U3kXCDYYhgZEcS36lIqcj5y6vYUUVM732/etCsuOLz6ppw=="
    }
  ]
}
```

### Encrypted format example

In this example a simple wireless network is added, but the file is encrypted
with the passphrase "test0000".

```
{
  "Cipher": "AES256",
  "Ciphertext": "eQ9/r6v29/83M745aa0JllEj4lklt3Nfy4kPPvXgjBt1eTByxXB+FnsdvL6Uca5JBU5aROxfiol2+ZZOkxPmUNNIFZj70pkdqOGVe09ncf0aVBDsAa27veGIG8rG/VQTTbAo7d8QaxdNNbZvwQVkdsAXawzPCu7zSh4NF/hDnDbYjbN/JEm1NzvWgEjeOfqnnw3PnGUYCArIaRsKq9uD0a1NccU+16ZSzyDhX724JNrJjsuxohotk5YXsCK0lP7ZXuXj+nSR0aRIETSQ+eqGhrew2octLXq8cXK05s6ZuVAc0mFKPkntSI/fzBACuPi4ZaGd3YEYiKzNOgKJ+qEwgoE39xp0EXMZOZyjMOAtA6e1ZZDQGWG7vKdTLmLKNztHGrXvlZkyEf1RDs10YgkwwLgUhm0yBJ+eqbxO/RiBXz7O2/UVOkkkVcmeI6yh3BdL6HIYsMMygnZa5WRkd/2/EudoqEnjcqUyGsL+YUqV6KRTC0PH+z7zSwvFs2KygrSM7SIAZM2yiQHTQACkA/YCJDwACkkQOBFnRWTWiX0xmN55WMbgrs/wqJ4zGC9LgdAInOBlc3P+76+i7QLaNjMovQ==",
  "HMAC": "3ylRy5InlhVzFGakJ/9lvGSyVH0=",
  "HMACMethod": "SHA1",
  "Iterations": 20000,
  "IV": "hcm6OENfqG6C/TVO6p5a8g==",
  "Salt": "/3O73QadCzA=",
  "Stretch": "PBKDF2",
  "Type": "EncryptedConfiguration"
}
```

### Recommended values example

In this example, the EAP Identity and Password are marked as recommended, i.e.
they can be edited by the user. All other values are mandatory.

```
{
  "Type": "UnencryptedConfiguration",
  "GlobalNetworkConfiguration": {},
  "NetworkConfigurations": [
    {
      "GUID": "{485e6176-dd34-6b6d-1234}",
      "Name": "wifi_test",
      "Type": "WiFi",
      "WiFi": {
        "SSID": "wifi_test",
        "Security": "WPA-EAP",
        "AutoConnect": true,
        "EAP": {
          "Inner": "MSCHAPv2",
          "Outer": "PEAP",
          "SaveCredentials": true,
          "UseSystemCAs": false,
          "Identity": "john-doe",
          "Password": "secret-password-123",
          "Recommended": ["Identity", "Password"]
        }
      }
    }
    }
  ]
}
```


## Standalone editor

The source code for a Chrome packaged app to generate ONC configuration can
be found here: https://chromium.googlesource.com/chromiumos/platform/spigots/

## Internationalization and Localization

UIs will need to have internationalization and localizations - the file
format will remain in English.

## Security Considerations

Data stored inside of open network configuration files is highly sensitive
to users and enterprises. The file format itself provides adequate
encryption options to allow standalone use-cases to be secure. For automatic
updates sent by policy, the policy transport should be made secure. The file
should not be stored unencrypted on disk as part of policy fetching and
should be cleared from memory after use.

## Privacy Considerations

Similarly to the security considerations, user names will be present in
these files for certain kinds of connections, so any places where the file
is transmitted or saved to disk should be secure. On client device, when
user names for connections that are user-specific are persisted to disk,
they should be stored in a location that is encrypted. Users can also opt in
these cases to not save their user credentials in the config file and will
instead be prompted when they are needed.

## Authors

* pneubeck@chromium.org
* stevenjb@chromium.org
