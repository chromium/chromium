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
