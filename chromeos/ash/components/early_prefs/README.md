"Early prefs" is the mechanism to export some of the Profile preferences
in a way that they can be read early in the ChromeOS Login flow, before
Profile loading starts.

EarlyPrefsWriter can be used to export policies from the Profile.
It works in a write-only mode, replacing previous data, so all
relevant values should be re-exported every time.

Version 1 of the schema:
 * schema_version (int)
 * data : dict from pref name to pref value and some metadata
   * managed (bool)
   * recommended (bool), defined only for managed prefs
   * value (base::Value) actual value of the pref
