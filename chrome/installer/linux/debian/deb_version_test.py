#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import deb_version

versions = [
    '0:~~~',
    '~~',
    '~',
    '',
    '1.2',
    '1.3',
    '99999',
    'a.1',
    'a.1-1',
    'a.1-2',
    'a.2',
    '.1',
    '1:~~~',
    '1:2:3',
    '1:abc',
    '1:abc:::---1',
    '1:zzz999',
    '2:~~~',
]

for i in range(len(versions)):
  for j in range(len(versions)):
    version_i = deb_version.DebVersion(versions[i])
    version_j = deb_version.DebVersion(versions[j])
    if i < j:
      assert version_i < version_j
      assert version_i <= version_j
      assert not version_i > version_j
      assert not version_i >= version_j
      assert not version_i == version_j
      assert version_i != version_j
    elif i > j:
      assert not version_i < version_j
      assert not version_i <= version_j
      assert version_i > version_j
      assert version_i >= version_j
      assert not version_i == version_j
      assert version_i != version_j
    else:
      assert not version_i < version_j
      assert version_i <= version_j
      assert not version_i > version_j
      assert version_i >= version_j
      assert version_i == version_j
      assert not version_i != version_j
