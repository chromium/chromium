# Open Network Configuration

The [Open Network Configuration
(ONC)](https://source.chromium.org/chromium/chromium/src/+/main:components/onc/docs/onc_spec.md;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
is a simple, open, but complete format that is used throughout ChromeOS to
describe network configurations. While typically associated with enterprise
policy, this format is used by all ChromeOS devices regardless of enrollment
status. Nonetheless, it is important to remember that ONC is what enables
enterprise policy to be capable of network configuration on ChromeOS.

## Background

There are many layers involved when thinking about networking on ChromeOS
(see more
[here](https://chromium.googlesource.com/chromium/src/+/master/chromeos/ash/components/network/README.md))),
multiple of which involve ONC. Of these layers there are two areas in particular
that are the most helpful to be familiar with and can be thought of as the
"implementation" of ONC within ChromeOS: **ONC constants** and the **translation
to/from ONC**.

## ONC Constants

The [ONC
specification](https://chromium.googlesource.com/chromium/src/+/HEAD/components/onc/docs/onc_spec.md)
is defined in code in the `chromium` codebase. This specification documents
every property that can be used in ONC, and where it can be used. For example,

```
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
```

This section of ONC indicates that an ONC network configuration, when its
**Type** property has the value of **Ethernet**, can have a property called
**Authentication** with a value of *None* or *8021X*. All of these fields, and
every field needed to support the complete ONC specification, are defined as
constants and organized such that they can easily be used by ChromeOS. These
constants can be found in
[//components/onc](https://source.chromium.org/chromium/chromium/src/+/main:components/onc/;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d).

## Translation to/from ONC

As mentioned earlier, there are many layers when it comes to networking on
ChromeOS and they do not all use ONC. In particular, the connection manager on
ChromeOS,
[Shill](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=31-62;drc=a95f7d7e85c88d5472b19bc4aac3413e3122117a),
does not use ONC and requires that ONC be translated to the Shill format before
being provided. This is by design; if Shill was tightly coupled with ONC it
would be difficult or impossible for Shill to ever change its internal
implementation.  For example, there could be reasons why Shill would want to
change which properties it exposes. By keeping ONC decoupled and making Chrome
responsible for translation Shill is able to freely make changes without
needing mirrored changes within ONC itself.

The core difference between the ONC and Shill formats is in the structure; ONC
is nested, similar to JSON, and the Shill format is flat. This can be more
clearly seen when inspecting the properties of a network using
[`chrome://network`](chrome://network) ([`os://network`](os://network) when
using
[Lacros](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/lacros.md)).

**ONC**
```
  "WiFi": {
    "BSSID": "13:37:13:37:13:37",
    "BSSIDAllowlist": [],
    "BSSIDRequested": ""
  },
```

**Shill**
```
  "WiFi.BSSID": "13:37:13:37:13:37",
  "WiFi.BSSIDAllowlist": [],
  "WiFi.BSSIDRequested": "",
```

The ONC format uses two separate keys, `WiFi` and `BSSID`, for the `BSSID`
property of a WiFi network whereas the Shill format uses a single key,
`WiFi.BSSID`. While the example above would be straightforward to translate
there are many unintuitive cases that need to be supported:

 * Verification that required properties exist and have valid values
 * Default values
 * Merging multiple ONC configurations, especially in regards to policy, to
   create a single configuration with no conflicting properties or values

This complexity is managed with the help of different classes and utility
functionality:

 * [Signatures](#signatures)
 * [`Validator`](#validator)
 * [Merging functionality](#merging-functionality)
 * [Translation tables](#translation-tables)
 * [Translation functionality](#translation-functionality)

### Signatures

The default values for all ONC properties are defined in the
[`onc_signature.h`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.h;drc=b6106a56da0170f037fe58840de60bb62cbad251)/[`onc_signature.cc`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.cc;drc=b6106a56da0170f037fe58840de60bb62cbad251)
files. These values are defined using
["signatures"](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.h;l=24-46;drc=f43f215636cf345e051c2ef30500ec1978ceac9a),
allowing for information beyond just the default value to be provided, e.g., the
type of the value itself. There are two distinct signature types that are
organized in a hierarchical structure that matches the ONC specification: a
"field" signature that is used for all ONC properties and a "value" signature
that is used to define the values of ONC properties. The "field" signatures
support nesting, and the "value" signatures do not. For example, the
[`EID`](https://source.chromium.org/chromium/chromium/src/+/main:components/onc/docs/onc_spec.md;l=1518-1521;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
property of a cellular network would be a "field" signature and the value, e.g.,
what the EID actually is, would be a "value" signature.

While the [commonly used value
signatures](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.cc;l=16-24;drc=f43f215636cf345e051c2ef30500ec1978ceac9a)
have already been defined, it is possible to create custom signatures when
necessary. For an example please see the [IP address config type
value](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.cc;l=367-368;drc=f43f215636cf345e051c2ef30500ec1978ceac9a).

Note: The Shill constants that ONC properties are mapped to can be found in
[dbus-constants.h](https://osscs.corp.google.com/chromium/chromium/src/+/main:third_party/cros_system_api/dbus/shill/dbus-constants.h;drc=c13d041e8414a890e2f24863a121c639d33237c2).

### `Validator`

TODO: Complete
[`Validator`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/components/onc/onc_validator.h;drc=7b5b3a0b8c5a14562035b7660b36aeb021cf98be)
section.

### Merging functionality

TODO: Complete [Merging
functionality](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_merger.h;drc=c12ecb80242110efc2852881f03b0924ec525fb2)
section.

### Translation tables

The [ONC translation
tables](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translation_tables.h;drc=cfbf492a6067da8111cb984972e39a7890e9895d)
define the mapping of ONC properties to Shill properties and provide helper
functions for translating properties. The tables allow properties with a direct
mapping to automatically be translated. However, while **all** properties should
be included in these tables, some properties require additional logic to be
translated correctly. When additional logic is required the property is expected
to be commented out, and to have documentation on the extra translation logic
required.

### Translation functionality

Building upon the concepts and functionality discussed earlier in this document,
there are [two
functions](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translator.h;drc=2fbe9cc6ca4ce563e2aa20361b011230ddd4afcb)
that are used to translate ONC to/from Shill properties.  Both functions accept
an [ONC signature](#translation-tables) that indicates to the translation logic
at which level this translation is occurring; this feature allows clients to
translate to and from any level of properties rather than needing to translate
only from top-level ONC to top-level Shill properties.

These functions,
[`TranslateONCObjectToShill`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translator_onc_to_shill.cc;l=679-686;drc=f43f215636cf345e051c2ef30500ec1978ceac9a)
and
[`TranslateShillServiceToONCPart`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translator_shill_to_onc.cc;l=1085-1095;drc=f43f215636cf345e051c2ef30500ec1978ceac9a),
internally perform translation using the [translation tables](#translation
tables) and [signatures](#signatures) by default but are implemented such that
custom translation logic can easily be added.

## Configuring Networks With ONC

While users typically will not be configuring networks using ONC, there are
three primary places that ONC is used within ChromeOS:

 * The
   [`ManagedNetworkConfigurationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=243-301;drc=3fae3cf5c30202296035e102951627745dc39627)
   class provides multiple APIs that are used for creating and configuring
   networks on ChromeOS. Unlike other networking classes, the
   `ManagedNetworkConfigurationHandler` class specifically only accepts ONC with
   a stated goal of decoupling users from direct interaction with Shill.
 * The `chrome://network` page is useful for debugging network issues and
   inspecting the properties of networks. However, this page also provides a
   mechanism to provide ONC to ChromeOS; try using this approach to configure
   your next network!
 * The UI that is used by administrators to managed enterprise enrolled devices
   provides the functionality to configure networks. These network
   configurations are translated to ONC before being sent via policy to ChromeOS
   devices.

## Extending ONC

TODO: Complete Extending ONC section.
