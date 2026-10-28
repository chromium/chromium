---
name: privacy/content-settings-remove-expired-pref-cleanup
description: >-
  When a CL touches the content settings providers, points out existing
  obsolete-pref cleanup entries whose own comment states a removal date that
  has already passed.
rubric: privacy
severity: nit
---

**Rule.** When a CL touches the content settings providers, point out existing
pref-cleanup code whose own comment says its removal date has passed.

Removed content settings leave dated cleanup entries behind (see
`privacy/content-settings-clear-prefs-of-removed-settings`): a `kObsolete*Pref`
constant, its registration in `RegisterProfilePrefs`, and a `ClearPref` in
`DiscardOrMigrateObsoletePreferences` of `PrefProvider` and `DefaultProvider`,
or a `Migrate*Exceptions` method there or in `HostContentSettingsMap`, each with
a comment like `// Deprecated 10/2026. Remove after 10/2027.` Once that date
passes, the whole entry can go.

Triggered by any change to `content_settings_pref_provider.cc`,
`content_settings_default_provider.cc`, or `host_content_settings_map.cc`. Read
the obsolete-pref constants, `DiscardOrMigrateObsoletePreferences`, and the
`Migrate*` methods in the post-CL file, including untouched lines. Compare each
stated date against today (milestones: against `MAJOR` in `chrome/VERSION`). If
any have expired, leave one comment listing them and suggesting deletion, here
or in a follow-up.

Only entries with an explicit date or milestone are in scope. Do not infer age
from blame, a `TODO(crbug.com/NNNN)`, or knowledge of when the feature was
removed; undated entries have no agreed deadline. Do not comment if the CL
deletes the expired entries itself, or the comment says to keep it ("DO NOT
REMOVE").

**Anchor.** Anchor on the first expired entry's `ClearPref` line, or its
declaration if there is none.

**Why.** The cleanup list only works if it is pruned; otherwise it grows forever
and registers prefs no profile has carried for years. The date is the agreed
signal that pruning is safe, and a CL already in these files is the cheapest
moment to act on it.

## Examples

Bad:

```cpp
// Reviewed 10/2026, untouched by the CL.
// Deprecated 03/2025. Remove after 03/2026.
constexpr char kObsoleteBarExceptionsPref[] =
    "profile.content_settings.exceptions.bar";
...
  prefs_->ClearPref(kObsoleteBarExceptionsPref);
```

Bad:

```cpp
// chrome/VERSION says MAJOR=158.
// TODO(crbug.com/NNNN): Remove this migration in M150.
void PrefProvider::MigrateBarExceptions() {
```

Good:

```cpp
// Reviewed 10/2026; not yet due.
// Deprecated 10/2026. Remove after 10/2027.
constexpr char kObsoleteFooExceptionsPref[] =
    "profile.content_settings.exceptions.foo";
```

Good:

```cpp
// Undated: out of scope, however old the setting is.
const char kObsoletePluginsDefaultPref[] =
    "profile.default_content_setting_values.plugins";
```

Good:

```cpp
// The CL deletes the expired entry itself.
-// Deprecated 03/2025. Remove after 03/2026.
-constexpr char kObsoleteBarExceptionsPref[] =
-    "profile.content_settings.exceptions.bar";
 ...
-  prefs_->ClearPref(kObsoleteBarExceptionsPref);
```
