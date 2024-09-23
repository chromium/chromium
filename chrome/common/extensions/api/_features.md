# Extension Features Files

[TOC]

## Summary

The Extension features files specify the different requirements for extension
feature availability.

An **extension feature** can be any component of extension capabilities. Most
notably, this includes extension APIs, but there are also more structural or
behavioral features, such as web accessible resources or event pages.

## Files

There are four different feature files used:
* [\_api\_features](https://chromium.googlesource.com/chromium/src/+/main/chrome/common/extensions/api/_api_features.json):
Specifies the requirements for API availability. If an extension doesn't satisfy
the requirements, the API will not be accessible in the extension's code.
* [\_permission\_features](https://chromium.googlesource.com/chromium/src/+/main/chrome/common/extensions/api/_permission_features.json):
Specifies the requirements for permission availability. If an extension doesn't
satisfy the requirements, the permission will not be granted and the extension
will have an install warning.
* [\_manifest\_features](https://chromium.googlesource.com/chromium/src/+/main/chrome/common/extensions/api/_manifest_features.json):
Specifies the requirements for manifest entry availability. If an extension
doesn't satisfy the requirements, the extension will fail to load with an error.
* [\_behavior\_features](https://chromium.googlesource.com/chromium/src/+/main/extensions/common/api/_behavior_features.json):
Specifies the requirements for miscellaneous extension behaviors. This should
typically not be used.

Note that these files may be present under chrome/common/extensions/api, as well
as under extensions/common/api and extensions/shell/common/api.

## Grammar

The feature files are written in JSON. Each file contains a single JSON object
with properties for each feature.

```
{
  "feature1": <definition>,
  "feature2": <definition>,
  ...
}
```

### Simple and Complex Features

Most features are known as "simple" features. These are features whose
definition is a single object that contains the properties describing the
criteria for availability. A simple feature might look like this:
```
"feature1": {
  "dependencies": ["permission:feature1"],
  "contexts": ["privileged_extension"]
}
```
`feature1` has a single definition, which says for it to be available, a
permission must be present and it must be executed from a privileged context.
(These concepts are covered more later in this document.)

Features can also be "complex". A complex feature has a list of objects to
specify multiple groups of possible properties. A complex feature could look
like this:
```
"feature1": [{
  "dependencies": ["permission:feature1"],
  "contexts": ["privileged_extension"]
}, {
  "dependencies": ["permission:otherPermission"],
  "contexts": ["privileged_extension", "unprivileged_extension"]
}]
```

With complex features, if either of the definitions are matched, the feature
is available (in other words, the feature definitions are logically OR'd
together). Complex features should frequently be avoided, as it makes the
logic more involved and slower.

### Inheritance

By default, features inherit from parents. A feature's ancestry is specified by
its name, where a child feature is the parent's name followed by a '.' and the
child's name. That is, `feature1.child` is the child of `feature1`. Inheritance
can carry for multiple levels (e.g. `feature1.child.child`), but this is rarely
(if ever) useful.

A child feature inherits all the properties of its parent, but can choose to
override them or add additional properties. Take the example:
```
"feature1": {
  "dependencies": ["permission:feature1"],
  "contexts": ["privileged_extension"]
},
"feature1.child": {
  "contexts": ["unprivileged_extension"],
  "extension_types": ["extension"]
}
```

In this case, `feature1.child` will effectively have the properties
```
"dependencies": ["permission:feature1"], # inherited from feature1
"contexts": ["unprivileged_extension"],     # inherited value overridden by child
"extension_types": ["extension]          # specified by child
```

If you don't want a child to inherit any features from the parent, add the
property `"noparent": true`. This is useful if, for instance, you have a
prefixed API name that isn't dependent on the prefix, such as app.window
(which is fully separate from the app API).

If the parent of a feature is a complex feature, the feature system needs to
know which parent to inherit from. To do this, add the property
`"default_parent": true` to one of the feature definitions in the parent
feature.

## Properties

The following properties are supported in the feature system.

### alias

The `alias` property specifies that the feature has an associated alias feature.
An alias feature is a feature that provides the same functionality as it's
source feature (i.e. the feature referenced by the alias). For example, an API
alias provides bindings for the source API under a different name. If one wanted
to declare an API alias, they would have to introduce an API alias feature -
defined as a feature that has `source` property, and set `alias` property on
the original feature. For example, the following would introduce an API alias
feature named `featureAlias` for API `feature`:
```none
{
  "feature": {
    "contexts": ["privileged_extension"],
    "channel": "dev",
    "alias": "featureAlias"
  },
  "featureAlias": {
   "contexts": ["privileged_extension"],
   "channel": "dev",
   "source": "feature"
  }
}
```
`featureAlias[source]` value specifies that `featureAlias` is an alias for API
feature `feature`

`feature[alias]` value specifies that `feature` API has an API alias
`featureAlias`

When feature `featureAlias` is available, `feature` bindings would be accessible
using `feauteAlias`. In other words `chrome.featureAlias` would point to an API
with the bindings equivalent to the bindings of `feature` API.

The alias API will inherit the schema from the source API, but it will not
respect the source API child features. To accomplish parity with the source API
feature children, identical child features should be added for the alias API.

Note that to properly create an alias, both `source` property on the alias
feature and `alias` property on the aliased feature have to be set.

Alias features are only available for API features, and each API can have at
most one alias.
For complex features, `alias` property will be set to the `alias` value of the
first component simple feature that has it set.

### blocklist

The `blocklist` property specifies a list of ID hashes for extensions that
cannot access a feature. See ID Hashes in this document for how to generate
these hashes.

Accepted values are lists of id hashes.

### channel

The `channel` property specifies a maximum channel for the feature availability.
That is, specifying `dev` means that the feature is available on `dev`,
`canary`, and `trunk`.

Accepted values are a single string from `trunk`, `canary`, `dev`, `beta`, and
`stable`.

### command\_line\_switch

The `command_line_switch` property specifies a command line switch that must be
present for the feature to be available.

Accepted values are a single string for the command line switch (without the
preceeding '--').

### feature\_flag

The `feature_flag` property specifies the name of a `base::Feature` flag that
must be enabled for the feature to be available. This can be used to implement a
remote kill switch for the feature. These feature flags should be defined at
[feature_flags.cc](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/features/feature_flags.cc).

Accepted value is a single string for the feature flag.

### component\_extensions\_auto\_granted

The `component_extensions_auto_granted` specifies whether or not component
extensions should be automatically granted access to the feature. By default,
this is `true`.

The only accepted value is the bool `false` (since true is the default).

### contexts

The `contexts` property specifies which JavaScript contexts can access the
feature. All API features must specify at least one context, and only API
features can specify contexts. The only exception to this are dummy namespaces
like `manifestTypes` etc. which can specify an empty list as its `contexts`
property.

Accepted values are a list of strings from `privileged_extension`,
`privileged_web_page`, `content_script`, `extension_service_worker`,
`lock_screen_extension`, `web_page`, `webui`, `webui_untrusted`, and
`unprivileged_extension`.

The `lock_screen_extension` context is used instead of `privileged_extension`
context for extensions on the Chrome OS lock screen. Other extensions related
contexts (`privileged_web_page`, `content_script`, `extension_service_worker`,
`unprivileged_extension`) are not expected to be present on the lock screen.

### default\_parent

The `default_parent` property specifies a feature definition from a complex
feature to be used as the parent for any children. See also Inheritance.

The only accepted value is the bool `true`.

### dependencies

The `dependencies` property specifies which other features must be present in
order to access this feature. This is useful so that you don't have to
re-specify all the same properties on an API feature and a permission feature.

A common practice is to put as many restrictions as possible in the
permission or manifest feature so that we warn at extension load, and put
relatively limited properties in an API feature with a dependency on the
manifest or permission feature.

To specify a dependent feature, use the prefix the feature name with the type
of feature it is, followed by a colon. For example, in order to specify a
dependency on a permission feature `foo`, we would add the dependency entry
`permission:foo`.

Accepted values are lists of strings specifying the dependent features.

### extension\_types

The `extension_types` properties specifies the different classes of extensions
that can use the feature.  It is very common for certain features to only be
allowed in certain extension classes, rather than available to all types.

Accepted values are lists of strings from `extension`, `hosted_app`,
`legacy_packaged_app`, `platform_app`, `shared_module`, `theme`, and
`login_screen_extension`.

### location

The `location` property specifies the required install location of the
extension.

Accepted values are a single string from `component`, `external_component`,
`policy`, and `unpacked`.

### internal

The `internal` property specifies whether or not a feature is considered
internal to Chromium. Internal features are not exposed to extensions, and can
only be used from Chromium code.

The only accepted value is the bool `true`.

### matches

The `matches` property specifies url patterns which should be allowed to access
the feature. Only API features may specify `matches`, and `matches` only make
sense with a context of either `webui` or `web_page`.

Accepted values are a list of strings specifying the match patterns.

### max\_manifest\_version

The `max_manifest_version` property specifies the maximum manifest version to be
allowed to access a feature. Extensions with a greater manifest version cannot
access the feature.

The only accepted value is `1`, as currently the highest possible manifest
version is `2`.

### min\_manifest\_version

The `min_manifest_version` property specifies the minimum manifest version to be
allowed to access a feature. Extensions with a lesser manifest version cannot
access the feature.

Accepted values are `2` and `3`, as 3 is currently the highest possible manifest
version.

### noparent

The `noparent` property specifies that a feature should not inherit any
properties from a derived parent. See also Inheritance.

The only accepted value is the bool `true`.

### platforms

The `platforms` property specifies the properties the feature should be
available on.

The accepted values are lists of strings from `chromeos`, `lacros`, `linux`,
`mac`, and `win`.

### requires\_delegated\_availability\_check

The `requires_delegated_availability_check` property specifies whether the
feature should determine its availability through a delegated check. Delegated
checks for a feature can be added to the `ExtensionsClient`, which will add the
delegated check methods to the specified feature name when initialized.

The only accepted value is the bool `true`. Omitting the value is equivalent to
`false`.

### session\_types

The `session_types` property specifies in which types of sessions a feature
should be available. The session type describes the type of user that is
logged in the current session. Session types to which feature can be restricted
are only supported on Chrome OS - features restricted to set of session types
will be disabled on other platforms. Also, note that all currently supported
session types imply that a user is logged into the session (i.e. features that
use `session_types` property will be disabled when a user is not logged in).

The accepted values are lists of strings from `regular`, `kiosk` and
`kiosk.autolaunched`.

`regular` session is a session launched for a regular, authenticated user.

`kiosk` session is a session launched for a kiosk app - an app that runs on its
own, in full control of the current session.

`kiosk.autolaunched` represents auto-launched kiosk session - a kiosk session
that is launched automatically from Chrome OS login screen, without any user
interaction. Note that allowing `kiosk` session implies allowing
`kiosk.autolaunched` session.

### source

The `source` property specifies that the feature is an alias for the feature
specified by the property value, and is only allowed for API features.
For more information about alias features, see [alias](#alias) property
documentation.

For complex features, `source` property will be set to the `source` value of the
first component simple feature that has it set.

### allowlist

The `allowlist` property specifies a list of ID hashes for extensions that
are the only extensions allowed to access a feature.

Accepted values are lists of id hashes.

## ID Hashes

Instead of listing the ID directly in the allowlist or blocklist section, we
use an uppercased SHA1 hash of the id.

To generate a new allowlist ID for an extension ID, do the following in bash:
```
$ echo -n "aaaabbbbccccddddeeeeffffgggghhhh" | \
     sha1sum | tr '[:lower:]' '[:upper:]'
```
(Replacing `aaaabbbbccccddddeeeeffffgggghhhh` with your extension ID.)

The output should be something like:
```
9A0417016F345C934A1A88F55CA17C05014EEEBA  -
```

Add the ID to the allowlist or blocklist for the desired feature. It is also
often useful to link the crbug next to the id hash, e.g.:
```
"allowlist": [
  "9A0417016F345C934A1A88F55CA17C05014EEEBA"  // crbug.com/<num>
]
```

Google employees: please update http://go/chrome-api-whitelist to map hashes
back to ids.

## Feature Contexts

A Feature Context is the type of JavaScript context that a feature can be made
available in. This allows us to restrict certain features to only being
accessible in more secure contexts, or to expose features to contexts outside
of extensions.

For each of these contexts, an "extension" context can refer to a context of
either an app or an extension.

### Privileged Extension Contexts

The `privileged_extension` context refers to a JavaScript context running from an
extension process. These are typically the most secure JavaScript contexts, as
it reduces the likelihood that a compromised web page renderer will have access
to secure APIs.

Traditionally, only pages with a top-level extension frame (with a
`chrome-extension://` scheme), extension popups, and app windows were privileged
extension contexts. With [site isolation](https://www.chromium.org/developers/design-documents/site-isolation),
extension frames running in web pages are also considered privileged extension
contexts, since they are running in the extension process (rather than in the
same process as the web page).

### Privileged Web Page Contexts

The `privileged_web_page` context refers to a JavaScript context running from a
hosted app. These are similar to privileged extension contexts in that they are
(partially) isolated from other processes, but are typically more restricted
than privileged extension processes, since hosted apps generally have fewer
permissions. Note that these contexts are unaffected by the `matches` property.

### Content Script Contexts

The `content_script` context refers to a JavaScript context for an extension
content script. Since content scripts share a process with (and run on the same
content as) web pages, these are considered very insecure contexts. Very few
features should be exposed to these contexts.

### Service Worker Contexts

The `extension_service_worker` context refers to a JavaScript context for an
extension's service worker. An extension can only register a service worker for
it's own domain, and these should only be run within an extension process. Thus,
these have similar privilege levels to privileged extension processes.

### Web Page Contexts

The `web_page` context refers to a JavaScript context for a simple web page,
completely separate from extensions. This is the least secure of all contexts,
and very few features should be exposed to these contexts. When specifying this
context, an accompanying URL pattern should be provided with the `matches`
property.

### WebUI Contexts

The `webui` context refers to a JavaScript context for a page with WebUI
bindings, such as internal chrome pages like chrome://settings or
chrome://extensions. These are considered secure contexts, since they are
an internal part of chrome. When specifying this context, an accompanying URL
pattern should be provided with the `matches` property.

### Unprivileged Extension Contexts

The `unprivileged_extension` context refers to a JavaScript context for an
extension frame that is embedded in an external page, like a web page, and
runs in the same process as the embedder. Given the limited separation between
the (untrusted) embedder and the extension frame, relatively few features are
exposed in these contexts. Note that with [site isolation](https://www.chromium.org/developers/design-documents/site-isolation),
extension frames (even those embedded in web pages) run in the trusted
extension process, and become privileged extension contexts.

## Compilation

The feature files are compiled as part of the suite of tools in
//tools/json\_schema\_compiler/. The output is a set of FeatureProviders that
contain a mapping of all features.

In addition to being significantly more performant than parsing the JSON files
at runtime, this has the added benefit of allowing us to validate at compile
time rather than needing a unittest (or allowing incorrect features).

In theory, invalid features should result in a compilation failure; in practice,
the compiler is probably missing some cases.

## Still to come

TODO(devlin): Add documentation for extension types. Probably also more on
requirements for individual features.
