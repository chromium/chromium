# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import platform
import subprocess

import packaging


def check_chrome_version(downloaded_chrome: str) -> packaging.version.Version:
  host = get_hosted_platform()
  if host == 'win':
    cmd = ('powershell -command "&{(Get-Item'
            '\''+ downloaded_chrome + '\').VersionInfo.ProductVersion}"')
    version = subprocess.run(cmd, check=True,
                          capture_output=True).stdout.decode('utf-8')
  else:
    cmd = [downloaded_chrome, '--version']
    version = subprocess.run(cmd, check=True,
                          capture_output=True).stdout.decode('utf-8')
    # Only return the version number portion
    version = version.strip().split(' ')[-1]
  return packaging.version.parse(version)

@functools.lru_cache
def get_hosted_platform() -> str:
  """Returns the host platform.

  Returns: One of 'linux', 'win' and 'mac'.
  """
  host = platform.uname().system.lower()
  if host in ('win32', 'cygwin', 'windows'):
    return 'win'
  if host.startswith('linux'):
    return 'linux'
  if host == 'darwin':
    return 'mac'

  raise RuntimeError('Unknown or unsupported host platform (%s).' %
                     platform.uname())
