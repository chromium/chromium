# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
from typing import Optional

import pytest

from chrome.test.variations import test_utils


CIPD_ROOT = os.path.join(test_utils.SRC_DIR, '.pytest-cipd')


def _install_cipd_packages(packages):
  logging.info('Installing packages in %s', CIPD_ROOT)
  if not os.path.exists(CIPD_ROOT):
    os.makedirs(CIPD_ROOT)
  ensure_path = os.path.join(CIPD_ROOT, '.ensure')
  with open(ensure_path, 'w') as ensure_file:
    ensure_file.write('$ParanoidMode CheckIntegrity\n\n')
    for package_name, version in packages:
      ensure_file.write(f'{package_name} {version}\n')
      logging.info(f'Adding package {package_name} {version}')

  ensure_cmd = [
      'cipd',
      'ensure',
      '-ensure-file',
      ensure_path,
      '-root',
      CIPD_ROOT,
  ]
  subprocess.check_call(ensure_cmd)


def _cas_ui_url(instance, digest):
  return (
    f'https://cas-viewer.appspot.com/projects/{instance}/instances/'
    f'default_instance/blobs/{digest}/tree'
  )


def pytest_addoption(parser):
  parser.addoption('--cipd-packages',
                   dest='cipd_packages',
                   help='A string containing the list of CIPD packages to '
                   'install. Each package is defined as name=version and '
                   'separated by ",".')

  parser.addoption('--cas-digests',
                   dest='cas_digests',
                   help='A string containing the list of CAS hashes to '
                   'download. Each hash is defined as instance=hash and '
                   'separated by ",".')

  parser.addoption('--cas-out-dir',
                   default=test_utils.SRC_DIR,
                   dest='cas_out_dir',
                   help='The path to the output dir where the CAS digests are '
                   'being downloaded to. Defaults to the root folder where '
                   'the chromium source is checked out.')


@pytest.fixture(scope="session")
def cipd_root(pytestconfig) -> str:
  '''Returns the root to the cipd where the packages are installed.'''
  cipd_packages = pytestconfig.getoption('cipd_packages')
  if cipd_packages:
    _install_cipd_packages(
      [tuple(each.split('=')) for each in cipd_packages.split(',')])
  return CIPD_ROOT


@pytest.fixture(scope="session", autouse=True)
def cas_digests(pytestconfig, cipd_root) -> Optional[str]:
  digests = pytestconfig.getoption('cas_digests')
  if not digests:
    return None

  cas_bin = os.path.join(cipd_root, 'cas')
  if test_utils.get_hosted_platform() == 'win':
    cas_bin = cas_bin + '.exe'
  assert os.path.exists(cas_bin), "The CAS binary is required."

  cas_output = pytestconfig.getoption('cas_out_dir')
  if not os.path.isabs(cas_output):
    cas_output = os.path.abspath(cas_output)
  logging.info('downloading cas digests to: %s', cas_output)

  for digest in digests.split(','):
    instance_name, digest_hash = digest.split('=')
    cas_cmd = [
      cas_bin, 'download', '-cas-instance', instance_name,
      '-digest', digest_hash, '-dir', cas_output]
    logging.info('%s', ' '.join(cas_cmd))
    subprocess.check_call(cas_cmd)
    logging.info('downloading cas digest: %s', _cas_ui_url(instance_name,
                                                           digest_hash))
  return cas_output
