# Android bindings for the Omnibox Component.

This folder contains declaration (data types, interfaces) enabling the Omnibox
Component native code and the Omnibox Android Java UI to exchange data.

Except for test and test support code, all classes hosted by this folder
represent API between the Omnibox component and the UI. This should be
limited to concrete data classes, interfaces, and enums, with minimal
processing.

Any Android specific code that does not fit the categories above (including
views, animations, data processing, interface implementations) should reside
under:
- `//chrome/browser/android/omnibox` (native classes),
- `//chrome/browser/ui/android/omnibox` (java classes).
