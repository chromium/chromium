# Sign-in Prefs Internals (`chrome://signin-internals`)

## Objective
The **Sign-in prefs** tab of `chrome://signin-internals` lets developers and QA inspect, edit and delete the per-account signin prefs of the current profile (the `signin.accounts_metadata_dict` dictionary pref, keyed by Gaia ID) without hand-editing the `Preferences` file.

## Registry & Accessor
- `SigninPrefsRegistry` (`components/signin/public/base/signin_prefs_registry.{h,cc}`) lists all supported account-scoped signin prefs in `kRegisteredSigninPrefs`. Each entry maps a structured path (`parent_keys` + `key`; a `'.'` inside a key is not a separator) to a `base::Value::Type`, optionally flagged as a timestamp. `DICT` entries only group other prefs.
- `SigninPrefsAccessor` owns the dictionary and `CHECK`s every read and write against the registry.
- Both are restricted to `SigninPrefs` and `SigninPrefsInternalsHandler` via `base::PassKey` and GN `visibility`.

## WebUI Message Contract
`SigninPrefsInternalsHandler` answers `cr.sendWithPromise()` messages for the profile hosting the page:
- `getSigninPrefs()`: `{canEdit, accountEmails, prefs}`, i.e. whether editing is enabled, the emails of the accounts that may have signin prefs (the accounts of `signin::GetAllGaiaIdsForKeyedPreferences()`: cookie jar, including signed-out accounts, primary account and refresh tokens; an unknown email is empty), and the per-account pref values.
- `getSupportedSigninPrefs()`: the schema of the registered prefs, as `{parentKeys, key, type}` where `type` is a `PrefType`.
- `setSigninPref(gaiaId, parentKeys, key, value)` and `deleteSigninPref(gaiaId, parentKeys, key)`: write or remove a registered pref (deleting a `DICT` removes its subtree) of an account listed in `accountEmails` or already present in the prefs. `setSigninPref` resolves with no value; `deleteSigninPref` resolves with whether the pref was set and removed.

Renderer inputs are validated before reaching `SigninPrefsAccessor`: invalid ones reject the promise with an error string without touching prefs. Values are type-checked but not range-checked.

## Security & Adding New Prefs
Editing requires `chrome://chrome-urls/#internal-debug-pages` (`chrome_urls::kInternalOnlyUisEnabled`) and a regular user profile (`profiles::IsRegularUserProfile()`); otherwise mutations reject with `"Editing is disabled"`. A new pref only needs to be registered in `kRegisteredSigninPrefs`: `getSupportedSigninPrefs()` exposes it automatically.
