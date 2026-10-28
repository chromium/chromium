---
name: privacy/content-settings-clear-prefs-of-removed-settings
description: >-
  Checks that a CL removing a content setting registration also clears the
  profile prefs that stored its exceptions and default, and dates that cleanup
  code so it can be pruned later.
rubric: privacy
severity: should_fix
---

**Rule.** A CL that removes a content setting must also delete the prefs that
stored it, and date that cleanup code.

A removal is a `Register(ContentSettingsType::FOO, "foo", ...)` call deleted
from the `Init()` of any settings registry in
`components/content_settings/core/browser/` (`WebsiteSettingsRegistry`,
`ContentSettingsRegistry`, `PermissionSettingsRegistry`, or any registry added
later), usually alongside renaming the enum value in
`content_settings_types.mojom` to `DEPRECATED_FOO`. Changing the name string
(`"foo"` to `"foo-devices"`) also counts: pref names derive from it.

Every registered setting, whichever registry it came from, gets prefs named
after it (`-` becomes `_`):

- `profile.content_settings.exceptions.<name>`, the per-site exceptions
  (`PrefProvider`, `content_settings_pref_provider.cc`).
- `profile.default_content_setting_values.<name>`, the user's default
  (`DefaultProvider`, `content_settings_default_provider.cc`).

Once the `Register` call is gone, nothing registers, reads, or deletes these
prefs; the user's data sits in `Preferences` forever. So the CL must, in each
provider:

1. Add a `kObsoleteFoo{Exceptions,Default}Pref` constant with the old name.
2. Register it in the "Obsolete prefs" block of `RegisterProfilePrefs`
   (`RegisterDictionaryPref`; defaults use `RegisterIntegerPref(..., 0)`).
3. `ClearPref` it in `DiscardOrMigrateObsoletePreferences`.
4. Keep the original registration's `BUILDFLAG` platform guards.
5. Date it: `// Deprecated 10/2026. Remove after 10/2027.` One year, as in
   `chrome/browser/prefs/browser_prefs.cc`.

If the CL migrates the data to another setting instead (compare
`PrefProvider::MigrateGeolocationExceptions`), the old pref must still be
cleared afterwards (`ClearAllContentSettingsRules()`), and the migration dated.

Do not flag if the CL, or a CL linked from its description, already clears the
prefs; or if the setting demonstrably never wrote them: never enabled for users,
or settable only via enterprise policy (`PolicyProvider`) or `WebUIAllowlist`.
If that is plausible but not stated, flag as a question to the author. If only
the date comment is missing, flag only that.

**Anchor.** Anchor on the `DEPRECATED_FOO` line in
`content_settings_types.mojom` if the CL adds one, otherwise on the removed
`Register` hunk in the registry.

**Why.** Site exceptions and defaults record which sites a user trusted or
distrusted. Once the setting is gone, no code path will ever delete them. The
date keeps the cleanup list itself from growing forever.

## Examples

Bad:

```cpp
// The CL's only content_settings change. "profile.content_settings.
// exceptions.foo" and "profile.default_content_setting_values.foo" are
// orphaned.
-  Register(ContentSettingsType::FOO, "foo", CONTENT_SETTING_ASK,
-           WebsiteSettingsInfo::SYNCABLE,
-           ...
-           PermissionSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY);
```

Bad:

```cpp
// Cleanup added, but undated.
+constexpr char kObsoleteFooExceptionsPref[] =
+    "profile.content_settings.exceptions.foo";
 ...
+  prefs_->ClearPref(kObsoleteFooExceptionsPref);
```

Good:

```cpp
// content_settings_pref_provider.cc
+// Deprecated 10/2026. Remove after 10/2027.
+constexpr char kObsoleteFooExceptionsPref[] =
+    "profile.content_settings.exceptions.foo";
 ...
+  registry->RegisterDictionaryPref(kObsoleteFooExceptionsPref);
 ...
+  prefs_->ClearPref(kObsoleteFooExceptionsPref);

// content_settings_default_provider.cc
+// Deprecated 10/2026. Remove after 10/2027.
+constexpr char kObsoleteFooDefaultPref[] =
+    "profile.default_content_setting_values.foo";
 ...
+  registry->RegisterIntegerPref(kObsoleteFooDefaultPref, 0);
 ...
+  prefs_->ClearPref(kObsoleteFooDefaultPref);
```

Good:

```cpp
// Migrated, old pref cleared, migration dated.
// TODO(crbug.com/NNNN): Remove this migration after 10/2027.
void PrefProvider::MigrateFooExceptions() {
  if (off_the_record_ || prefs_->GetBoolean(kFooMigratedPref)) {
    return;
  }
  auto* old_pref = GetPref(ContentSettingsType::DEPRECATED_FOO);
  auto* new_pref = GetPref(ContentSettingsType::FOO_DEVICES);
  ...  // Copy rules from old_pref to new_pref.
  old_pref->ClearAllContentSettingsRules();
  prefs_->SetBoolean(kFooMigratedPref, true);
}
```

Good:

```cpp
// CL description: "FOO was only settable via the FooAllowedForUrls enterprise
// policy (PolicyProvider), so no profile prefs hold data for it."
-  Register(ContentSettingsType::FOO, "foo", CONTENT_SETTING_ALLOW,
            ...
```
