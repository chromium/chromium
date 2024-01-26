# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import attr
import pytest

from chrome.test.variations import test_utils
from chrome.test.variations.fixtures.result_sink import AddTag

@attr.define(frozen=True)
class TestOptions:
  platform: str = attr.attrib()
  channel: str = attr.attrib()
  chrome_version: str = attr.attrib()
  changelist: int = attr.attrib()

def pytest_addoption(parser):
  # By default, running on the hosted platform.
  parser.addoption('--target-platform',
                   default=test_utils.get_hosted_platform(),
                   dest='target_platform',
                   choices=['linux', 'win', 'mac', 'android', 'webview',
                            'android_webview', 'cros', 'lacros'],
                   help='If present, run for the target platform, '
                   'defaults to the host platform.')

  parser.addoption('--channel',
                   default='dev',
                   choices=['dev', 'canary', 'beta', 'stable', 'extended'],
                   help='The channel of Chrome to download.')

  parser.addoption('--chrome-version',
                   dest='chrome_version',
                   help='The version of Chrome to download. '
                   'If this is set, --channel will be ignored.')

  parser.addoption('--changelist',
                   dest='changelist',
                   help='google3 changelist number (optional).')

@pytest.fixture(scope="session")
def test_options(pytestconfig) -> TestOptions:
  return TestOptions(
    platform=pytestconfig.getoption('target_platform'),
    channel=pytestconfig.getoption('channel'),
    chrome_version=pytestconfig.getoption('chrome_version'),
    changelist=pytestconfig.getoption('changelist')
  )

@pytest.fixture(autouse=True)
def tag_test_options(test_options, add_tag:AddTag) -> None:
  # Add test parameters to result logs.
  add_tag('platform', test_options.platform)
  add_tag('channel', test_options.channel)
  if test_options.chrome_version:
    add_tag('chrome_version', test_options.chrome_version)
  if test_options.changelist:
    add_tag('changelist', test_options.changelist)
