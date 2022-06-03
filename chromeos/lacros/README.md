This directory contains the lacros-chrome client implementation of the ChromeOS
API (//chromeos/crosapi). This is primarily glue code that allows lacros-chrome
to access interfaces exposed by ash-chrome.

Examples of contained functionality include:
* Device interaction (e.g. querying displays)
* System provided UI components (e.g. file picker)

The closest equivalent on other operating systems would be a client-side library
that provides OS-specific functionality, such as Win32 or Cocoa.
