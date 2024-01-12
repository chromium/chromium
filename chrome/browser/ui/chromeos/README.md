This holder contains code that is used for both Lacros and Ash UI but not on
other platforms. This should only include the common interface or
implementation between Lacros and Ash. Platform-specific implementation should
live in chrome/browser/ui/lacros for Lacros and chrome/browser/ui/ash for Ash.

Non-UI code belongs in chrome/browser/chromeos.
