# Chromium for Android Settings

## Getting Started

The Android developer [Settings
guide](https://developer.android.com/guide/topics/ui/settings) is the best place
to start before contributing to Chromium for Android's settings.

## Helper Classes

Many common utility functions that are useful for developing settings screens in
Chromium for Android can be found in `//components/browser_ui/settings/android`.

## Widgets

The `widget` subdirectory contains a number of extensions of AndroidX
[Preference](https://developer.android.com/reference/androidx/preference/Preference)
classes that provide Chromium-specific behavior (like Managed preferences) or
common Chromium UI components (like buttons).

The base Preference classes included in the AndroidX Preference library can also
be used directly in Chromium for Android Settings screens.

## Naming conventions

Historically, the term “preferences” is heavily overloaded in Chromium for
Android. Some disambiguations are already conventional in Chromium for Android,
but are not always applied consistently, so we document those usages explicitly
below.

Because the screens shown above tend to be informally referred to as “Settings”
(and are labeled as such in the application), going forward we will prefer the
term “Settings” in code over “Preferences” to refer to classes and target names
dealing with these UIs.

More information see
[go/clank-preferences-refactor](go/clank-preferences-refactor).

| “Preferences” Usage | Disambiguation |
|---------------------|-------------------|
| Activity name, as in Preferences.java | Use "Settings" i.e. SettingsActivity |
| Preference fragment, extending `PreferenceFragment` | Suffix with 'Settings'.Fragment suffix is still also acceptable i.e. 'SettingsFragment' |
| Individual preferences, extending `androidx.preference.Preference` | No change, adhere to [ Android conventions](https://developer.android.com/guide/topics/ui/settings) |
| Collection of individual preferences in XML, used with `addPreferencesFromResource()` | No change, adhere to [ Android conventions](https://developer.android.com/guide/topics/ui/settings) |
| Android SharedPreferences, as in `android.content.SharedPreferences` | Always keep “Shared” prefix |
| Chrome Native preferences, as in `components/prefs/pref_service.h` | Always refer to with the abbreviated “prefs”, adhering to Chrome conventions |
