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
    update.py            Regenerates the lock files (developer tool).
    runtime.lock.<arch>.json
                         .deb URLs, sizes and SHA256s for amd64 and arm64;
                         the only input to build.py.
    build.py             Builds the CIPD package (3pp builder).
    ubuntu-archive-keyring.gpg
  linux-amd64/           CIPD package checkouts (not checked in).
  linux-arm64/
```

The pipeline is:

```
Ubuntu archive -> update.py (apt) -> runtime.lock.<arch>.json -> build.py -> CIPD
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
`runtime.lock.<arch>.json` with a new version. It does this for every
architecture (`amd64` and `arm64`) unless `--arch` limits it.
`snapshot.ubuntu.com/ubuntu` serves all architectures, including arm64, which
the live archive only has on `ports.ubuntu.com`.

It needs `apt-get`, `apt-cache` and `unsquashfs` on the host and downloads
`core26`, `gnome-core26-sdk` and `gtk-common-themes` from the snap store
(about 700 MB; use `--cache-dir` to keep them).

## build.py

The 3pp packager builder of each platform (`linux-amd64`, `linux-arm64`) runs
`build.py`, which takes the architecture from `$_3PP_PLATFORM` (or `--arch`),
and:

* downloads the `.deb`s listed in `runtime.lock.<arch>.json` from
  `snapshot.ubuntu.com` and checks their sizes and SHA256s against the lock
  file,
* extracts them, applies the prime filters and the cleanup list from the lock
  file, and
* uploads the result to CIPD as
  `chromium/chrome/installer/linux/snap/runtime/linux-<arch>`.

The package contains `prime/` (files for the root of the snap), `copyright/`
(the `debian/copyright` file of every bundled package) and `manifest.json`.
`//DEPS` checks it out to `linux-<arch>/` when the gclient variable
`checkout_snap_runtime` is set to `True` (`linux-arm64/` also needs `arm64` in
`target_cpu`), e.g. in `.gclient`:

```
solutions = [
  {
    ...
    "custom_vars": {
      "checkout_snap_runtime": True,
    },
  },
]
```

The variable also sets the default of the GN arg `enable_snap_package`, so
x64 and arm64 builds in such a checkout build the snap package along with the
`.deb` and `.rpm` packages. The `linux-rel` builders set it through the
`checkout_snap_runtime` gclient config of the chromium recipe module.

## Updating

For example, for Ubuntu security updates:

1. Run `3pp/update.py [--snapshot <YYYYMMDDTHHMMSSZ>]`.
2. Land the change. The `3pp-linux-amd64-packager` try builder checks it on the
   CQ, and the `3pp-linux-amd64-packager` and `3pp-linux-arm64-packager` CI
   builders upload the new versions within 6 hours.
3. Update the version in `//DEPS`.
