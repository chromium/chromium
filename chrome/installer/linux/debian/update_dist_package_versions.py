#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Downloads package lists and records package versions into
dist_package_versions.json.
"""

import binascii
import cStringIO
import gzip
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib2

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

SUPPORTED_DEBIAN_RELEASES = {
    'Debian 10 (Buster)': 'buster',
    # TODO: bullseye-updates does not have a .gz file.
    # 'Debian 11 (Bullseye)': 'bullseye',
}

SUPPORTED_UBUNTU_RELEASES = {
    'Ubuntu 18.04 (Bionic)': 'bionic',
    'Ubuntu 20.04 (Focal)': 'focal',
}

PACKAGE_FILTER = set([
    "libatspi2.0-0",
    "libasound2",
    "libatk1.0-0",
    "libatk-bridge2.0-0",
    "libc6",
    "libcairo2",
    "libcups2",
    "libdbus-1-3",
    "libdrm2",
    "libexpat1",
    "libgbm1",
    "libgcc1",
    "libglib2.0-0",
    "libnspr4",
    "libnss3",
    "libpango-1.0-0",
    "libpangocairo-1.0-0",
    "libstdc++6",
    "libuuid1",
    "libx11-6",
    "libx11-xcb1",
    "libxcb1",
    "libxcb-dri3-0",
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
])

def create_temp_file_from_data(data):
  file = tempfile.NamedTemporaryFile()
  file.write(data)
  file.flush()
  return file

if sys.platform != 'linux2':
  print >> sys.stderr, "Only supported on Linux."
  sys.exit(1)

deb_sources = {}
for release in SUPPORTED_DEBIAN_RELEASES:
  codename = SUPPORTED_DEBIAN_RELEASES[release]
  deb_sources[release] = [
      {
          "base_url": url,
          "packages": [ "main/binary-amd64/Packages.gz" ]
      } for url in [
          "http://ftp.us.debian.org/debian/dists/%s" % codename,
          "http://ftp.us.debian.org/debian/dists/%s-updates" % codename,
          "http://security.debian.org/dists/%s/updates" % codename,
      ]
  ]
for release in SUPPORTED_UBUNTU_RELEASES:
  codename = SUPPORTED_UBUNTU_RELEASES[release]
  repos = ['main', 'universe']
  deb_sources[release] = [
      {
          "base_url": url,
          "packages": [
              "%s/binary-amd64/Packages.gz" % repo for repo in repos
          ],
      } for url in [
          "http://us.archive.ubuntu.com/ubuntu/dists/%s" % codename,
          "http://us.archive.ubuntu.com/ubuntu/dists/%s-updates" % codename,
          "http://security.ubuntu.com/ubuntu/dists/%s-security" % codename,
      ]
  ]

distro_package_versions = {}
package_regex = re.compile('^Package: (.*)$')
version_regex = re.compile('^Version: (.*)$')
for distro in deb_sources:
  package_versions = {}
  for source in deb_sources[distro]:
    base_url = source["base_url"]
    release = urllib2.urlopen("%s/Release" % base_url).read()
    release_gpg = urllib2.urlopen("%s/Release.gpg" % base_url).read()
    keyring = os.path.join(SCRIPT_DIR, 'repo_signing_keys.gpg')
    release_file = create_temp_file_from_data(release)
    release_gpg_file = create_temp_file_from_data(release_gpg)
    subprocess.check_output(['gpgv', '--quiet', '--keyring', keyring,
                             release_gpg_file.name, release_file.name])
    for packages_gz in source["packages"]:
      gz_data = urllib2.urlopen("%s/%s" % (base_url, packages_gz)).read()

      sha = hashlib.sha256()
      sha.update(gz_data)
      digest = binascii.hexlify(sha.digest())
      matches = [line for line in release.split('\n')
                 if digest in line and packages_gz in line]
      assert len(matches) == 1

      zipped_file = cStringIO.StringIO()
      zipped_file.write(gz_data)
      zipped_file.seek(0)
      contents = gzip.GzipFile(fileobj=zipped_file, mode='rb').read()
      package = ''
      for line in contents.split('\n'):
        if line.startswith('Package: '):
          match = re.search(package_regex, line)
          package = match.group(1)
        elif line.startswith('Version: '):
          match = re.search(version_regex, line)
          version = match.group(1)
          if package in PACKAGE_FILTER:
            package_versions[package] = version
  distro_package_versions[distro] = package_versions

missing_any_package = False
for distro in distro_package_versions:
  missing_packages = PACKAGE_FILTER.difference(distro_package_versions[distro])
  if missing_packages:
    missing_any_package = True
    print >> sys.stderr, "Packages are not avilable on %s: %s" % (
        distro, ', '.join(missing_packages))
if missing_any_package:
  sys.exit(1)

with open(os.path.join(SCRIPT_DIR, 'dist_package_versions.json'), 'w') as f:
  f.write(json.dumps(distro_package_versions, sort_keys=True, indent=4,
                     separators=(',', ': ')))
  f.write('\n')
