# Site Isolation support in components/site_isolation/

This directory provides support for [Site
Isolation](https://www.chromium.org/Home/chromium-security/site-isolation/)
that cannot live in //content yet needs to be shared across platforms that
may not necessarily include //chrome. This mostly includes mechanisms for
partial Site Isolation, such as loading the built-in list of isolated
sites, managing preferences that store heuristically isolated sites where
users have entered passwords or logged in via OAuth, and computing memory
thresholds for applying Site Isolation. Platforms that currently use
partial Site Isolation include Android, Fuchsia, WebLayer, and Blink for iOS,
while iOS (with Webkit) does not support Site Isolation at all. See
[process_model_and_site_isolation.md](/docs/process_model_and_site_isolation.md)
for more details.

Most of the core Site Isolation code can be found in
[content/browser/](/content/browser/).
