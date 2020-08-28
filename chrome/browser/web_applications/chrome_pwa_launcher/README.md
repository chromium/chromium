This directory contains files for chrome_pwa_launcher.exe, a
Progressive-Web-App launcher needed to enable PWAs to register as file-type
handlers on Windows. When run from a PWA's directory, chrome_pwa_launcher.exe
runs chrome.exe and passes it the PWA's ID (passed to the launcher by the PWA's
file-handler-registration path in the Windows registry) to launch the PWA.

Each PWA has an individual hardlink or copy (if hardlinking fails) of
chrome_pwa_launcher.exe in its web-app directory (User Data\<profile>\Web
Applications\<app ID>), which the browser registers with Windows as a file
handler for file types the PWA accepts. An individual per-PWA hardlink/copy is
required because Windows de-duplicates applications with the same executable
path in the Open With menu.

Important features of chrome_pwa_launcher.exe include:
* Generic icon representing "Chrome PWA", needed on Windows 7 where setting a
custom icon for hardlinks/copies is not possible;
* Filename renamed to the PWA's display name on Windows 7, where setting a
custom display name for hardlinks/copies is not possible and the filename is
used instead;
* Pass launcher version to chrome.exe to check for updates: hardlinks/copies
pass their version as a command-line switch, which chrome.exe compares to its
own version and replaces all PWA launchers with new hardlinks/copies of the
latest chrome_pwa_launcher.exe if needed; and
* Simple crash logging: the last launch result is recorded in the registry to
be read by chrome.exe the next time it runs, avoiding the overhead of full
Crashpad logging while providing basic information on whether
chrome_pwa_launcher.exe is working.

chrome_pwa_launcher.exe assumes that it is run from a web-app data directory
(User Data\<profile>\Web Applications\<app ID>), as it uses the "Last Browser"
file in its great-grandparent User Data directory to find the chrome.exe to
launch.

NOTE: PWA launchers must be compiled in a non-component build to support file
handling. Component-build EXEs will hit a missing-DLL error if run from the
web-app directory.
