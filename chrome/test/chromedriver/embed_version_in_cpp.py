#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds version string in C++ code for ChromeDriver."""

import optparse
import os
import re
import sys

import chrome_paths
import cpp_source

sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'build', 'util'))
import lastchange


def get_release_version(chrome_version_file, version_info):
  """Return version string appropriate for a release branch.

  Args:
    chrome_version_file: name of Chrome's version file, e.g., chrome/VERSION.
    version_info: VersionInfo object returned from lastchange.FetchVersionInfo.
  """

  # Parse Chrome version file, which should have four lines of key=value,
  # giving the major, minor, build, and patch number.
  version = {}
  for line in open(chrome_version_file, 'r').readlines():
    key, val = line.rstrip('\r\n').split('=', 1)
    version[key] = val

  if version_info is not None:
    # Release branch revision has the format
    # '26c10db8bff36a8b6fc073c0f38b1e9493cabb04-refs/branch-heads/3515@{#5}'.
    match = re.match('[0-9a-fA-F]+-refs/branch-heads/\d+@{#\d+}',
                     version_info.revision)
    if not match:
      # revision is not the expected format, probably not in a release branch.
      return None

    # Result is based on Chrome version number, e.g.,
    # '70.0.3516.0 (26c10db8bff36a8b6fc073c0f38b1e9493cabb04)'.
    return '%s.%s.%s.%s (%s)' % (
        version['MAJOR'], version['MINOR'], version['BUILD'], version['PATCH'],
        version_info.revision_id)
  else:
    # No version_info from Git. Assume we are in a release branch if Chrome
    # patch number is not 0.
    if version['PATCH'] == '0':
      return None

    return '%s.%s.%s.%s' % (
        version['MAJOR'], version['MINOR'], version['BUILD'], version['PATCH'])


def get_master_version(chromedriver_version, version_info):
  """Return version string appropriate for the master branch.

  Args:
    chromedriver_version: ChromeDriver version, e.g., '2.41'.
    version_info: VersionInfo object returned from lastchange.FetchVersionInfo.
  """

  if version_info is None:
    return None

  # Master branch revision has the format
  # 'cc009559c91323445dec7e2f545298bf10726eaf-refs/heads/master@{#581331}'.
  # We need to grab the commit position (e.g., '581331') near the end.
  match = re.match('[0-9a-fA-F]+-refs/heads/master@{#(\d+)}',
                   version_info.revision)

  if not match:
    # revision is not the expected format, probably not in the master branch.
    return None

  # result is based on legacy style ChromeDriver version number, e.g.,
  # '2.41.581331 (cc009559c91323445dec7e2f545298bf10726eaf)'.
  commit_position = match.group(1)
  return '%s.%s (%s)' % (
      chromedriver_version, commit_position, version_info.revision_id)


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--chromedriver-version-file')
  parser.add_option('', '--chrome-version-file')
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h  file should be created')
  options, _ = parser.parse_args()

  chromedriver_version = open(
      options.chromedriver_version_file, 'r').read().strip()

  # Get a VersionInfo object corresponding to the Git commit we are at,
  # using the same filter used by main function of build/util/lastchange.py.
  # On success, version_info.revision_id is a 40-digit Git hash,
  # and version_info.revision is a longer string with more information.
  # On failure, version_info is None.
  version_info = lastchange.FetchGitRevision(None, '^Change-Id:')

  version = get_release_version(options.chrome_version_file, version_info)

  if version is None:
    version = get_master_version(chromedriver_version, version_info)

  if version is None:
    if version_info is not None:
      # Not in a known branch, but has Git revision.
      version = '%s (%s)' % (chromedriver_version, version_info.revision_id)
    else:
      # Git command failed. Just use ChromeDriver version string.
      version = chromedriver_version

  global_string_map = {
      'kChromeDriverVersion': version
  }
  cpp_source.WriteSource('version',
                         'chrome/test/chromedriver',
                         options.directory, global_string_map)


if __name__ == '__main__':
  sys.exit(main())
