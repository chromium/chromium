# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Paths, ports, and output locations shared by the media perf tests.

Importing this module also puts the helper directories that the perf tests
import from (build/util, fuchsia_web/av_testing, build/fuchsia/test) on
sys.path.
"""

import argparse
import os
import shutil
import sys

REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..')
)
BUILD_UTIL_ROOT = os.path.join(REPO_ROOT, 'build', 'util')
CHROME_FUCHSIA_ROOT = os.path.join(REPO_ROOT, 'fuchsia_web', 'av_testing')
TEST_SCRIPTS_ROOT = os.path.join(REPO_ROOT, 'build', 'fuchsia', 'test')
CROSSBENCH_ROOT = os.path.join(REPO_ROOT, 'third_party', 'crossbench')
CROSSBENCH_VPYTHON_SPEC = os.path.join(CROSSBENCH_ROOT, 'vpython.toml')
# Set in the re-executed process so it does not re-execute again.
_REEXEC_ENV = 'MEDIA_PERF_CROSSBENCH_PYTHON'

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


def _is_cros_sender(argv: list[str]) -> bool:
    """Returns whether `argv` asks for a ChromeOS sender."""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--sender-os')
    args, _ = parser.parse_known_args(argv)
    return args.sender_os == 'cros'


def reexec_with_crossbench_python(script_path: str) -> None:
    """Re-runs `script_path` under Crossbench's vpython spec for ChromeOS.

    The ChromeOS sender imports Crossbench in-process, and Crossbench pins a
    newer Python than Chromium's root vpython.toml, which test_env.py uses to
    start the test. ChromeOS runs therefore use Crossbench's spec, which also
    has every package the harness imports. Other senders do not import
    Crossbench, so they stay on the root spec and a Crossbench roll cannot
    break them.

    Must be called before starting any child process, since the current
    process image is replaced. Returns without doing anything for other
    senders and in the re-executed process.

    Raises:
        RuntimeError: vpython3 is not on PATH.
    """
    if os.environ.get(_REEXEC_ENV) or not _is_cros_sender(sys.argv[1:]):
        return
    vpython = shutil.which('vpython3')
    if not vpython:
        raise RuntimeError(
            'vpython3 is not on PATH, so the test cannot run under '
            f'{CROSSBENCH_VPYTHON_SPEC}.'
        )
    argv = [
        vpython,
        '-vpython-spec',
        CROSSBENCH_VPYTHON_SPEC,
        os.path.abspath(script_path),
        *sys.argv[1:],
    ]
    sys.stdout.flush()
    sys.stderr.flush()
    os.execve(vpython, argv, {**os.environ, _REEXEC_ENV: '1'})
