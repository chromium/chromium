# Snap runtime

The snap package (`//chrome/installer/linux/snap`) is based on `core26` and
bundles a few Ubuntu 26.04 ("resolute") libraries that the base snap does not
provide, such as NSS, libsecret, opensc-pkcs11 and Qt 6. snapcraft used to
fetch them as `stage-packages`. This directory builds them as a CIPD package
instead, so that the snap can be built without snapcraft and with pinned
inputs.

```
runtime/
  README.md
  PRESUBMIT.py
  3pp/                   3pp spec (see //docs/cipd_and_3pp.md)
    3pp.pb
    config.json          Packages and prime filters; input to update.py.
    update.py            Regenerates runtime.lock.json (developer tool).
    runtime.lock.json    .deb URLs, sizes and SHA256s; the only input to
                         build.py.
    build.py             Builds the CIPD package (3pp builder).
    ubuntu-archive-keyring.gpg
  linux-amd64/           CIPD package checkout (not checked in).
```

The pipeline is:

```
Ubuntu archive -> update.py (apt) -> runtime.lock.json -> build.py -> CIPD
```

## update.py

`update.py` resolves the packages in `config.json` with `apt-get` at a
`snapshot.ubuntu.com` timestamp (`resolute`, `resolute-updates` and
`resolute-security`). apt runs in a private root (its own `APT_CONFIG`, `Dir`
and dpkg status file in a temporary directory), so it doesn't touch the host's
apt configuration and doesn't need root. apt checks the archive signature
against `ubuntu-archive-keyring.gpg` (the "Ubuntu Archive Automatic Signing
Key (2018)", `F6EC B376 2474 EDA9 D21B  7022 8719 20D1 991B C93C`, from the
ubuntu-keyring package) and the hashes of the indices and `.deb`s.

Like snapcraft on a `core26` build host, the packages of the `core26` base snap
are marked as installed, and Recommends are not installed. `update.py` then
lists the files that the snapcraft gnome extension's cleanup parts would
remove because the content snaps (`gnome-core26-sdk`, `gtk-common-themes`,
the mesa content snap) already provide them, and writes everything to
`runtime.lock.json` with a new version.

It needs `apt-get`, `apt-cache` and `unsquashfs` on the host and downloads
`core26`, `gnome-core26-sdk` and `gtk-common-themes` from the snap store
(about 700 MB; use `--cache-dir` to keep them).

## build.py

The `3pp-linux-amd64-packager` builder runs `build.py`, which:

* downloads the `.deb`s listed in `runtime.lock.json` from `snapshot.ubuntu.com`
  and checks their sizes and SHA256s against the lock file,
* extracts them, applies the prime filters and the cleanup list from the lock
  file, and
* uploads the result to CIPD as
  `chromium/chrome/installer/linux/snap/runtime/linux-amd64`.

The package contains `prime/` (files for the root of the snap), `copyright/`
(the `debian/copyright` file of every bundled package) and `manifest.json`.
`//DEPS` will check it out to `linux-amd64/` when the gclient variable
`checkout_snap_runtime` is set, once the first version is uploaded.

## Updating

For example, for Ubuntu security updates:

1. Run `3pp/update.py [--snapshot <YYYYMMDDTHHMMSSZ>]`.
2. Land the change. The `3pp-linux-amd64-packager` try builder checks it on the
   CQ, and the CI builder uploads the new version within 6 hours.
3. Update the version in `//DEPS`.
