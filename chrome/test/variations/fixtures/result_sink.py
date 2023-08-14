# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import pytest

from chrome.test.variations.test_utils import SRC_DIR

# The module result_sink is under build/util and imported relative to its root.
sys.path.append(os.path.join(SRC_DIR, 'build', 'util'))

from lib.results import result_sink
from lib.results import result_types

_RESULT_TYPES = {
  'passed': result_types.PASS,
  'failed': result_types.FAIL,
  'skipped': result_types.SKIP,
}


def pytest_sessionstart(session):
  session.sink_client = result_sink.TryInitClient()


def pytest_sessionfinish(session, exitstatus: int):
  if session.sink_client:
    session.sink_client.close()


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
  sink_client = item.session.sink_client

  outcome = yield
  result = outcome.get_result()
  if result.when == 'call':
    if sink_client:
        sink_client.Post(result.nodeid, _RESULT_TYPES[result.outcome],
                        int(result.duration * 1000), result.caplog,
                        result.fspath)
    logging.info(f'posting result: {result.nodeid}, {result.duration}, '
                 f'{_RESULT_TYPES[result.outcome]}, {result.fspath}')
