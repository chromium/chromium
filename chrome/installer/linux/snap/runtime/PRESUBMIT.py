# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //chrome/installer/linux/snap/runtime."""

PRESUBMIT_VERSION = '2.0.0'

_3PP_DIR = '3pp/'
# One lock file per architecture: 3pp/runtime.lock.<arch>.json.
_LOCK_FILE_RE = r'^3pp/runtime\.lock\.[^/]+\.json$'


def CheckRuntimeUnittests(input_api, output_api):
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.os_path.join(input_api.PresubmitLocalPath(), _3PP_DIR),
        [r'^.+_unittest\.py$'],
        run_on_python2=False,
        skip_shebang_check=True,
    )


def _LockVersion(input_api, contents):
    if not contents:
        return None
    return input_api.json.loads('\n'.join(contents))['version']


def CheckVersionBumped(input_api, output_api):
    """The 3pp builder only rebuilds when the version in a lock changes."""
    if input_api.no_diffs:
        return []
    local_path = input_api.PresubmitLocalPath()
    changed = False
    locks = []
    for f in input_api.AffectedFiles(include_deletes=False):
        rel = input_api.os_path.relpath(f.AbsoluteLocalPath(), local_path)
        rel = rel.replace(input_api.os_path.sep, '/')
        if not rel.startswith(_3PP_DIR) or rel.endswith('_unittest.py'):
            continue
        changed = True
        if input_api.re.match(_LOCK_FILE_RE, rel):
            locks.append(f)
    if not changed:
        return []

    for lock in locks:
        old_version = _LockVersion(input_api, lock.OldContents())
        new_version = _LockVersion(input_api, lock.NewContents())
        if old_version is None or new_version != old_version:
            return []
    return [
        output_api.PresubmitError(
            'Changes to chrome/installer/linux/snap/runtime/%s need a new '
            '"version" in a %sruntime.lock.<arch>.json file (run '
            '"update.py"), otherwise the 3pp builder does not upload a new '
            'package.' % (_3PP_DIR, _3PP_DIR)
        )
    ]
