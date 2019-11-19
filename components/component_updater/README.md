# Component Updater

## Overview
The Component Updater is a piece of Chrome responsible for updating other pieces
of Chrome. It runs in the browser process and communicates with a set of servers
using the [Omaha](https://github.com/google/omaha) protocol to find the latest
versions of components, download them, and register them with the rest of
Chrome.

The primary benefit of components is that they can be updated without an update
to Chrome itself, which allows them to have faster (or desynchronized) release
cadences, lower bandwidth consumption, and avoids bloat in the (already sizable)
Chrome installer. The primary drawback is that they require Chrome to tolerate
their absence in a sane way.

In the normal configuration, the component updater registers all components
during (or close to) browser start-up, and then begins checking for updates six
minutes later, with substantial pauses between successive update application.

## Terminology
For the purposes of this document:

 * A `component` is any element of Chrome's core functionality that is sometimes
   delivered by the component updater separately from the browser itself,
   usually as a dynamically-linked library or data file.
 * A `crx file` is any file in the
   [CRX package format](https://developer.chrome.com/extensions/crx).

## Adding New Components
This document covers the work that must be done on the client side. Additional
work is necessary to integrate with the Omaha servers, and is covered in
[Google-internal documentation](http://go/newchromecomponent).

This assumes you've already done the hard work of splitting your functionality
out into a dynamically-linked library or data file.

### Create a CRX Package Signing Key & Manifest (Non-Google)
All components are delivered as CRX files (signed ZIP archives). You need to
create a signing key. If you are a Googler, follow the instructions at
http://go/newchromecomponent for maximum key security. Otherwise, you can
create an RSA key pair using `openssl` or a similar tool.

You will additionally need to create a manifest.json file. If nothing else, the
manifest file must specify the component's version and name. If you plan to
release the component using Google infrastructure, this file can be generated
for you automatically.

### Writing an Installer
The "installer" is a piece of Chrome code that the component updater will run to
install or update the component. Installers live at
`src/chrome/browser/component_updater`.

You will need the SHA256 hash of the public key generated in the previous step,
as well as the CRX ID, which consists of the first half (128 bits) of that hash,
rendered as hexadecimal using the characters `a-p` (rather than `0-9a-f`).

New components should use
[`component_installer`](component_installer.h)
if possible, as this provides you with transparent differential updates, version
management, and more. You must provide a `ComponentInstallerPolicy` object to
a new `ComponentInstaller`.
[file\_type\_policies\_component\_installer.cc](../../chrome/browser/component_updater/file_type_policies_component_installer.cc)
is a good example to work from.

Components need to be registered with the component updater. This is done in
[RegisterComponentsForUpdate](https://cs.chromium.org/chromium/src/chrome/browser/chrome_browser_main.cc).

### Bundle with the Chrome Installer (Optional)
If you need the guarantee that some implementation of your component is always
available, you must bundle a component implementation with the browser itself.
If you are using `ComponentInstaller`, you simply need to make sure that
your component implementation (and a corresponding manifest.json file) are
written to DIR\_COMPONENTS as part of the build. The manifest.json file must
state the version of this component implementation, and the files must be
bitwise identical to the contents of any update CRX with that version for that
platform, as the system will attempt to apply differential updates over these
files.

### Implement On-Demand or Just-In-Time Updates (Optional)
Contact the component\_updater OWNERS.
