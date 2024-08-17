#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import binascii
import gzip
import hashlib
import io
import json
import os
import sys
import urllib.request
import xml.etree.ElementTree as ET


LIBRARY_FILTER = {
    "ld-linux-x86-64.so",
    "libX11-xcb.so",
    "libX11.so",
    "libXcomposite.so",
    "libXcursor.so",
    "libXdamage.so",
    "libXext.so",
    "libXfixes.so",
    "libXi.so",
    "libXrandr.so",
    "libXrender.so",
    "libXss.so",
    "libXtst.so",
    "libasound.so",
    "libatk-1.0.so",
    "libatk-bridge-2.0.so",
    "libatspi.so.0",
    "libc.so",
    "libcairo.so",
    "libcups.so",
    "libdbus-1.so",
    "libdl.so",
    "libdrm.so.2",
    "libexpat.so",
    "libgbm.so.1",
    "libgcc_s.so",
    "libgdk-3.so",
    "libgio-2.0.so",
    "libglib-2.0.so",
    "libgmodule-2.0.so",
    "libgobject-2.0.so",
    "libm.so",
    "libnspr4.so",
    "libnss3.so",
    "libnssutil3.so",
    "libpango-1.0.so",
    "libpangocairo-1.0.so",
    "libpthread.so",
    "librt.so",
    "libsmime3.so",
    "libstdc++.so",
    "libudev.so",
    "libuuid.so",
    "libxcb-dri3.so.0",
    "libxcb.so",
    "libxkbcommon.so.0",
    "libxshmfence.so.1",
    "rtld(GNU_HASH)",
}

SUPPORTED_FEDORA_RELEASES = ["39", "40"]
SUPPORTED_OPENSUSE_LEAP_RELEASES = ["15.5"]

COMMON_NS = "http://linux.duke.edu/metadata/common"
RPM_NS = "http://linux.duke.edu/metadata/rpm"
REPO_NS = "http://linux.duke.edu/metadata/repo"

rpm_sources = {}
for version in SUPPORTED_FEDORA_RELEASES:
    rpm_sources["Fedora " + version] = [
        "https://download.fedoraproject.org/pub/fedora/linux/releases/%s/Everything/x86_64/os/"
        % version,
        # 'updates' must appear after 'releases' since its entries
        # overwrite the originals.
        "https://download.fedoraproject.org/pub/fedora/linux/updates/%s/Everything/x86_64/"
        % version,
    ]
for version in SUPPORTED_OPENSUSE_LEAP_RELEASES:
    rpm_sources["openSUSE Leap " + version] = [
        "https://download.opensuse.org/distribution/leap/%s/repo/oss/" %
        version,
        # 'update' must appear after 'distribution' since its entries
        # overwrite the originals.
        "https://download.opensuse.org/update/leap/%s/oss/" % version,
    ]

provides = {}
missing_any_library = False
for distro in rpm_sources:
    distro_provides = {}
    provided_prefixes = set()
    for source in rpm_sources[distro]:
        source = urllib.request.urlopen(source).geturl()

        response = urllib.request.urlopen(source + "repodata/repomd.xml")
        repomd = ET.fromstring(response.read())
        primary = (source +
                   repomd.find("./{%s}data[@type='primary']/{%s}location" %
                               (REPO_NS, REPO_NS)).attrib["href"])
        expected_checksum = repomd.find(
            "./{%s}data[@type='primary']/{%s}checksum[@type='sha256']" %
            (REPO_NS, REPO_NS)).text

        response = urllib.request.urlopen(primary)
        gz_data = response.read()

        sha = hashlib.sha256()
        sha.update(gz_data)
        actual_checksum = binascii.hexlify(sha.digest()).decode("ascii")
        assert expected_checksum == actual_checksum

        with gzip.open(io.BytesIO(gz_data), "rb") as f:
            contents = f.read().decode("utf-8")
        metadata = ET.fromstring(contents)
        for package in metadata.findall("./{%s}package" % COMMON_NS):
            if package.find("./{%s}arch" % COMMON_NS).text != "x86_64":
                continue
            package_name = package.find("./{%s}name" % COMMON_NS).text
            package_provides = []
            for entry in package.findall(
                    "./{%s}format/{%s}provides/{%s}entry" %
                (COMMON_NS, RPM_NS, RPM_NS)):
                name = entry.attrib["name"]
                for prefix in LIBRARY_FILTER:
                    if name.startswith(prefix):
                        package_provides.append(name)
                        provided_prefixes.add(prefix)
            distro_provides[package_name] = package_provides
    provides[distro] = sorted(
        list(
            set([
                package_provides for package in distro_provides
                for package_provides in distro_provides[package]
            ])))

    missing_libraries = LIBRARY_FILTER.difference(provided_prefixes)
    if missing_libraries:
        missing_any_library = True
        print(
            "Libraries are not available on %s: %s" %
            (distro, ", ".join(missing_libraries)),
            file=sys.stderr,
        )

if missing_any_library:
    sys.exit(1)

script_dir = os.path.dirname(os.path.realpath(__file__))
with open(os.path.join(script_dir, "dist_package_provides.json"), "w") as f:
    json.dump(provides, f, sort_keys=True, indent=4, separators=(",", ": "))
    f.write("\n")
