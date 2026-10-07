# Per-Account Preferences (`SigninPrefs`)

`SigninPrefs` manages per-account sign-in preferences stored in a dictionary
preference (`signin.accounts_metadata_dict`) keyed by `GaiaId`:

* [`signin_prefs_keys.h`](signin_prefs_keys.h): Dictionary key constants and
  parent key paths (`namespace signin::internal`).
* [`SigninPrefsRegistry`](signin_prefs_registry.h): Compile-time registry
  (`kRegisteredSigninPrefs`) of all active per-account preferences, their
  nesting path, and their expected `base::Value::Type`.
* [`SigninPrefsAccessor`](signin_prefs_accessor.h): `base::PassKey`-restricted
  storage layer that validates every read and write against
  `SigninPrefsRegistry`.
* [`SigninPrefs`](signin_prefs.h): Strongly-typed public API used by feature
  code, delegating to `SigninPrefsAccessor`.

## How to Add a Per-Account Preference

Adding a new per-account preference requires 4 steps in
`//components/signin/public/base/`:

### 1. Declare the key constant in `signin_prefs_keys.h`

Add the key constant to the active per-account preference keys section in
`namespace signin::internal`:

```cpp
// Pref to store the number of times MyNewPromo has been shown per account.
inline constexpr std::string_view kMyNewPromoShownCount =
    "MyNewPromoShownCount";
```

If the preference lives inside a nested sub-dictionary, reuse an existing parent
path array (such as `kAvatarButtonPromoParents` or
`kCrossDeviceHistoryPromoParents`) or declare a new
`std::to_array<std::string_view>` parent path.

### 2. Register the descriptor in `signin_prefs_registry.cc`

Add a `PrefDescriptor` entry to `kRegisteredSigninPrefs` in
`signin_prefs_registry.cc`:

```cpp
// Top-level integer preference:
{.key = signin::internal::kMyNewPromoShownCount,
 .type = base::Value::Type::INTEGER},

// Top-level timestamp preference (serialized via base::TimeToValue as STRING):
{.key = signin::internal::kMyNewPromoLastShownTime,
 .type = base::Value::Type::STRING,
 .is_timestamp = true},

// Nested preference under a registered sub-dictionary:
{.key = signin::internal::kMyNestedPromoCount,
 .parent_keys_storage =
     ToParentStorage(signin::internal::kAvatarButtonPromoParents),
 .type = base::Value::Type::INTEGER},
```

Compile-time `static_assert`s in `signin_prefs_registry.cc` verify that all
keys are non-empty and unique, types are valid (`BOOLEAN`, `INTEGER`, `STRING`,
or `DICT`), timestamps use `STRING`, parent dictionaries are registered as
`DICT`, and no active key is in `kDeprecatedSigninPrefs`.

### 3. Add typed accessors on `SigninPrefs` (`signin_prefs.{h,cc}`)

Expose strongly-typed methods on `SigninPrefs` and delegate to `accessor_`:

```cpp
void SigninPrefs::IncrementMyNewPromoShownCount(const GaiaId& gaia_id) {
  accessor_.IncrementIntPref(gaia_id, signin::internal::kMyNewPromoShownCount);
}

int SigninPrefs::GetMyNewPromoShownCount(const GaiaId& gaia_id) const {
  return accessor_.GetIntPref(gaia_id, signin::internal::kMyNewPromoShownCount);
}
```

### 4. Exercise the new accessor in `signin_prefs_unittest.cc`

Call the new setter/incrementer and getter in
`SigninPrefsTest.AllWrittenPrefsAreRegisteredWithMatchingType`. This test
verifies bidirectional synchronization between `SigninPrefs` and
`SigninPrefsRegistry`: every registered preference must be written by
`SigninPrefs` in the test, and every written entry must match its registered
descriptor.

## How to Deprecate a Per-Account Preference

When a per-account preference is no longer used:

1. **Remove the typed methods** from `SigninPrefs` (`signin_prefs.{h,cc}`) and
   their calls in `signin_prefs_unittest.cc`.
2. **Remove the descriptor** from `kRegisteredSigninPrefs` in
   `signin_prefs_registry.cc`.
3. **Deprecate the key in `signin_prefs_keys.h`**: move the constant to the
   `DEPRECATED prefs` section and add it to `kDeprecatedSigninPrefs` (for
   top-level account keys) so `SigninPrefs::MigrateObsoleteSigninPrefs()`
   automatically removes it from existing user profiles.
