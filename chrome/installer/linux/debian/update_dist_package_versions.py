#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Downloads package lists and records package versions into
dist_package_versions.json.
"""

import binascii
import hashlib
import io
import json
import lzma
import os
import re
import subprocess
import sys
import tempfile
import urllib.request

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

SUPPORTED_DEBIAN_RELEASES = {
    "Debian 11 (Bullseye)": "bullseye",
    "Debian 12 (Bookworm)": "bookworm",
}

SUPPORTED_UBUNTU_RELEASES = {
    "Ubuntu 18.04 (Bionic)": "bionic",
    "Ubuntu 20.04 (Focal)": "focal",
    "Ubuntu 22.04 (Jammy)": "jammy",
}

PACKAGE_FILTER = {
    "libasound2",
    "libatk-bridge2.0-0",
    "libatk1.0-0",
    "libatspi2.0-0",
    "libc6",
    "libcairo2",
    "libcups2",
    "libdbus-1-3",
    "libdrm2",
    "libexpat1",
    "libgbm1",
    # See the comment in calculate_package_deps.py about libgcc_s.
    # TODO(https://crbug.com/40549424): Add this once support for
    # Ubuntu Bionic is dropped.
    # "libgcc-s1",
    "libglib2.0-0",
    "libnspr4",
    "libnss3",
    "libpango-1.0-0",
    "libpangocairo-1.0-0",
    "libstdc++6",
    "libudev1",
    "libuuid1",
    "libx11-6",
    "libx11-xcb1",
    "libxcb-dri3-0",
    "libxcb1",
    "libxcomposite1",
    "libxcursor1",
    "libxdamage1",
    "libxext6",
    "libxfixes3",
    "libxi6",
    "libxkbcommon0",
    "libxrandr2",
    "libxrender1",
    "libxshmfence1",
    "libxss1",
    "libxtst6",
}


def create_temp_file_from_data(data):
    file = tempfile.NamedTemporaryFile()
    file.write(data)
    file.flush()
    return file


if not sys.platform.startswith("linux"):
    print("Only supported on Linux.", file=sys.stderr)
    sys.exit(1)

deb_sources = {}
for release in SUPPORTED_DEBIAN_RELEASES:
    codename = SUPPORTED_DEBIAN_RELEASES[release]
    deb_sources[release] = [{
        "base_url": url,
        "packages": ["main/binary-amd64/Packages.xz"]
    } for url in [
        "http://ftp.us.debian.org/debian/dists/%s" % codename,
        "http://ftp.us.debian.org/debian/dists/%s-updates" % codename,
        "http://security.debian.org/dists/%s-security/updates" % codename,
    ]]
for release in SUPPORTED_UBUNTU_RELEASES:
    codename = SUPPORTED_UBUNTU_RELEASES[release]
    repos = ["main", "universe"]
    deb_sources[release] = [{
        "base_url":
        url,
        "packages": ["%s/binary-amd64/Packages.xz" % repo for repo in repos],
    } for url in [
        "http://us.archive.ubuntu.com/ubuntu/dists/%s" % codename,
        "http://us.archive.ubuntu.com/ubuntu/dists/%s-updates" % codename,
        "http://security.ubuntu.com/ubuntu/dists/%s-security" % codename,
    ]]

distro_package_versions = {}
package_regex = re.compile("^Package: (.*)$")
version_regex = re.compile("^Version: (.*)$")
for distro in deb_sources:
    package_versions = {}
    for source in deb_sources[distro]:
        base_url = source["base_url"]
        with urllib.request.urlopen("%s/Release" % base_url) as response:
            release = response.read().decode("utf-8")
        with urllib.request.urlopen("%s/Release.gpg" % base_url) as response:
            release_gpg = response.read()
        keyring = os.path.join(SCRIPT_DIR, "repo_signing_keys.gpg")
        release_file = create_temp_file_from_data(release.encode("utf-8"))
        release_gpg_file = create_temp_file_from_data(release_gpg)
        subprocess.check_output([
            "gpgv",
            "--quiet",
            "--keyring",
            keyring,
            release_gpg_file.name,
            release_file.name,
        ])
        for packages_xz in source["packages"]:
            with urllib.request.urlopen("%s/%s" %
                                        (base_url, packages_xz)) as response:
                xz_data = response.read()

            sha = hashlib.sha256()
            sha.update(xz_data)
            digest = binascii.hexlify(sha.digest()).decode("utf-8")
            matches = [
                line for line in release.split("\n")
                if digest in line and packages_xz in line
            ]
            assert len(matches) == 1

            with lzma.open(io.BytesIO(xz_data), "rb") as f:
                contents = f.read().decode("utf-8")
            package = ""
            for line in contents.split("\n"):
                if line.startswith("Package: "):
                    match = re.search(package_regex, line)
                    package = match.group(1)
                elif line.startswith("Version: "):
                    match = re.search(version_regex, line)
                    version = match.group(1)
                    if package in PACKAGE_FILTER:
                        package_versions[package] = version
    distro_package_versions[distro] = package_versions

missing_any_package = False
for distro in distro_package_versions:
    missing_packages = PACKAGE_FILTER - set(distro_package_versions[distro])
    if missing_packages:
        missing_any_package = True
        print(
            "Packages are not available on %s: %s" %
            (distro, ", ".join(missing_packages)),
            file=sys.stderr,
        )
if missing_any_package:
    sys.exit(1)

with open(os.path.join(SCRIPT_DIR, "dist_package_versions.json"), "w") as f:
    json.dump(distro_package_versions,
              f,
              sort_keys=True,
              indent=4,
              separators=(",", ": "))
    f.write("\n")
