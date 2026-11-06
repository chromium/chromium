# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //chrome/installer/linux/snap/runtime."""

PRESUBMIT_VERSION = '2.0.0'

_3PP_DIR = '3pp/'
_LOCK_FILE = _3PP_DIR + 'runtime.lock.json'


def CheckRuntimeUnittests(input_api, output_api):
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.os_path.join(input_api.PresubmitLocalPath(), _3PP_DIR),
        [r'^.+_unittest\.py$'],
        run_on_python2=False,
        skip_shebang_check=True,
    )


def CheckVersionBumped(input_api, output_api):
    """The 3pp builder only rebuilds when the version in the lock changes."""
    local_path = input_api.PresubmitLocalPath()
    changed = False
    lock = None
    for f in input_api.AffectedFiles(include_deletes=False):
        rel = input_api.os_path.relpath(f.AbsoluteLocalPath(), local_path)
        rel = rel.replace(input_api.os_path.sep, '/')
        if not rel.startswith(_3PP_DIR) or rel.endswith('_unittest.py'):
            continue
        changed = True
        if rel == _LOCK_FILE:
            lock = f
    if not changed:
        return []

    new_version = None
    old_version = None
    if lock:
        new_version = input_api.json.loads('\n'.join(lock.NewContents()))[
            'version'
        ]
        old_contents = lock.OldContents()
        if old_contents:
            old_version = input_api.json.loads('\n'.join(old_contents))[
                'version'
            ]
    if lock and (old_version is None or new_version != old_version):
        return []
    return [
        output_api.PresubmitError(
            'Changes to chrome/installer/linux/snap/runtime/%s need a new '
            '"version" in %s (run "update.py"), otherwise the 3pp builder does '
            'not upload a new package.' % (_3PP_DIR, _LOCK_FILE)
        )
    ]
