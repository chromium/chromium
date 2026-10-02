# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Paths, ports, and output locations shared by the media perf tests.

Importing this module also puts the helper directories that the perf tests
import from (build/util, fuchsia_web/av_testing, build/fuchsia/test) on
sys.path.
"""

import os
import sys

REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..')
)
BUILD_UTIL_ROOT = os.path.join(REPO_ROOT, 'build', 'util')
CHROME_FUCHSIA_ROOT = os.path.join(REPO_ROOT, 'fuchsia_web', 'av_testing')
TEST_SCRIPTS_ROOT = os.path.join(REPO_ROOT, 'build', 'fuchsia', 'test')
CROSSBENCH_ROOT = os.path.join(REPO_ROOT, 'third_party', 'crossbench')

for _path in (BUILD_UTIL_ROOT, CHROME_FUCHSIA_ROOT, TEST_SCRIPTS_ROOT):
    if _path not in sys.path:
        sys.path.append(_path)

CFT_JSON_URL = (
    'https://googlechromelabs.github.io/chrome-for-testing/'
    'known-good-versions-with-downloads.json'
)

CHROMEDRIVER_PORT = int(os.environ.get('CHROMEDRIVER_PORT', '49573'))
SERVER_PORT = int(os.environ.get('SERVER_PORT', '8000'))
LOCAL_HOST_IP = '127.0.0.1'
REMOTE_URL = f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}'

_OUTPUT_DIR = os.environ.get('ISOLATED_OUTDIR', '/tmp')
RECORDINGS_DIR = os.path.join(_OUTPUT_DIR, 'recordings')
TRACES_DIR = os.path.join(_OUTPUT_DIR, 'traces')
INVOCATIONS_DIR = os.path.join(_OUTPUT_DIR, 'invocations')
