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
[here](https://chromium.googlesource.com/chromium/src/+/master/chromeos/ash/components/network/README.md)),
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
[Shill](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=31-62;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d),
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
[`onc_signature.h`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.h;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)/[`onc_signature.cc`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.cc;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
files. These values are defined using
["signatures"](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.h;l=24-46;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d),
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
signatures](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.cc;l=16-24;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
have already been defined, it is possible to create custom signatures when
necessary. For an example please see the [IP address config type
value](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_signature.cc;l=367-368;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d).

Note: The Shill constants that ONC properties are mapped to can be found in
[dbus-constants.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/cros_system_api/dbus/shill/dbus-constants.h;drc=0d7f441b3c48f9ebb5a7947fc0a0d4393541085b).

### `Validator`

ONC configurations are validated before being applied to ensure that they align
with the specification. The validation is performed by the [`Validator`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_validator.h;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
class; examples of issues this class checks for are:

* Field has a value with the incorrect type
* Field has an unknown enum value
* Field has an integer value that is out of range
* Field name is unknown
* Duplicate values for fields intended to be globally unique, e.g. GUID
* Required field is missing
* Recommended field contains the name of a field that was not provided in the
  configuration
* Recommended field exists but the configuration is not from enterprise policy

Please reference the ONC spec for [more
information](https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md#recommended-values)
and
[examples](https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md#recommended-values-example)
of recommended values.

There are different modes that can be used when performing validation of an ONC
configuration:

* Log warnings
* Error when an unknown field name is found
* Error when a field is unexpectedly recommended
* Enterprise policy

For more information on the impact these modes have when performing validation
see the [`Validator` class
documentation](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc/onc_validator.h;l=23-76;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d).

### Merging functionality

Multiple ONC configurations can be applied to a device; for example, a device
may have a "global" ONC configuration that is applied to all users of a device
and a "user" ONC configuration that is applied to a specific user. Similar to
how functionality is provided to check if a configuration is valid,
functionality is also provided to merge configurations:

* [`MergeSettingsAndPoliciesToEffective()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_merger.h;l=17-31;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
returns the result of merging the provided configurations. If conflicting values
are found when merging the provided configurations the "effective", or highest
priority value, will be chosen. For example, if a network is configured locally
on the DUT by the user to have auto-connect enabled but a global policy
restricts auto-connect the effective value of auto-connect will be disabled/off.
  * Please note that this function expects that the provided configurations
    refer to the same section/level of ONC.
* [`MergeSettingsAndPoliciesToAugmented()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_merger.h;l=33-47;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
  returns the result of merging the provided configurations, similar to
  `MergeSettingsAndPoliciesToEffective()`, but instead of providing the
  effective value for each field this function will provide the augmented
  values. The "augmented" value of a field is a dictionary that includes the
  effective value and can include additional values that provide additional
  context about the field such as the source of the different values and their
  enforcement.  For more information about the dictionary format of augmented
  values see the [managed ONC dictionary format
  section](https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md#dictionary-format)
  of the specification.

### Translation tables

The [ONC translation
tables](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translation_tables.h;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
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
functions](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translator.h;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
that are used to translate ONC to/from Shill properties. Both functions accept
an [ONC signature](#translation-tables) that indicates to the translation logic
at which level this translation is occurring; this feature allows clients to
translate to and from any level of properties rather than needing to translate
only from top-level ONC to top-level Shill properties.

These functions,
[`TranslateONCObjectToShill`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translator_onc_to_shill.cc;l=679-686;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
and
[`TranslateShillServiceToONCPart`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_translator_shill_to_onc.cc;l=1085-1095;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d),
internally perform translation using the [translation tables](#translation
tables) and [signatures](#signatures) by default but are implemented such that
custom translation logic can easily be added.

## Configuring Networks With ONC

While users typically will not be configuring networks using ONC, there are
three primary places that ONC is used within ChromeOS:

 * The
   [`ManagedNetworkConfigurationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=243-301;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
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

Extending ONC is straightforward but requires care to ensure that all of the
necessary changes are made. Additionally, it is important to consider when
making these changes is what the expected behavior is when an older client
*without* these changes receives a policy *with* these changes; this case is far
from uncommon and it is worth checking that no issues would be introduced.

Depending on how ONC is being extended not all of the changes mentioned are
required, but the comprehensive list is as follows:

 * Update
   [//components/onc/docs/onc_spec.md](https://chromium.googlesource.com/chromium/src/+/HEAD/components/onc/docs/onc_spec.md).
   Any changes to the specification are expected to have been discussed and
   reviewed prior to landing.
 * Add any necessary constants to [//components/onc/onc_constants.[h|cc]](https://source.chromium.org/chromium/chromium/src/+/main:components/onc;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d).
 * Update
   [//chromeos/components/onc/onc_signature.[h|cc]](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/components/onc;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
   files to have the correct type for all properties added. This is where custom
   default values can be chosen for properties.
 * Update the
   [//chromeos/ash/components/network/onc/onc_translation_tables.[h|cc]](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
   files to map all properties added to their corresponding Shill properties. This can
   only be done for properties that can be translated 1:1.
 * Update the
   [//chromeos/ash/components/network/onc/onc_translator.\*](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
   files to handle translating all properties that could not be handled
   automatically by the translation tables.
 * If necessary, update
   [//chromeos/ash/components/network/onc/onc_merger.cc](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/onc/onc_merger.cc;drc=f444c4385d414a4c916d9ed83fde775c484d0c2d)
   to correctly handle merging any added or changed properties.

### Examples

 * [This
   change](https://chromium-review.googlesource.com/c/chromium/src/+/4424729)
   introduces a new property to the ONC specification. The added property is
   intended to be mutually exclusive with another property, and to enforce this
   behavior the validators were updated to check that only one is provided.
 * [This
   change](https://chromium-review.googlesource.com/c/chromium/src/+/3794908)
   changes the default of an existing property to be a value other than the
   default value for the variable type; in this case the change makes a boolean
   property default to `true`.
