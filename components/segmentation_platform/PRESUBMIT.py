# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This presubmit ensures that all known test suites in the current component
are included in the launcher filter file."""

import re
from typing import Dict, List

TOOL_PATH = 'tools/testing/launcher_filter_file.py'


def GetToolPathRelativeToRepositoryRoot(input_api) -> str:
    # Returns the path to the launcher_filter_file.py tool, relative to the
    # chromium src directory to simplify copy-paste of a command on errors.
    presubmit_local_path = input_api.PresubmitLocalPath()
    repository_root = input_api.change.RepositoryRoot()
    common_path = input_api.os_path.commonpath(
        [repository_root, presubmit_local_path])
    component_relative_path = presubmit_local_path[len(common_path) + 1:]
    return input_api.os_path.join(component_relative_path, TOOL_PATH)


def FetchLauncherFilterFileData(input_api, cwd: str) -> Dict[str, str]:
    # Fetches the actual and expected filter file content so they can be
    # compared. Returns a dictionary that contains 'expected' and 'actual' as
    # comparable strings.
    try:
        old_sys_path = input_api.sys.path
        cwd = input_api.PresubmitLocalPath()
        input_api.sys.path += [input_api.os_path.join(cwd, 'tools')]
        from testing import launcher_filter_file as lff
        ret = {}
        ret['expected'] = lff.GetExpectedLauncherFilterFileContent()
        ret['actual'] = lff.GetActualLauncherFilterFileContent()
        return ret
    finally:
        input_api.sys.path = old_sys_path


def GetPylintConfiguration(input_api, output_api) -> List:
    disabled_warnings = [
        'C0415',  # import-outside-toplevel
    ]
    return input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        version='2.7',
        disabled_warnings=disabled_warnings)


def _CheckHistograms(input_api, output_api):
    """Checks if the histograms.txt file is up to date."""
    # Path to the directory containing segmentation platform models.
    model_dir = input_api.os_path.join('components', 'segmentation_platform',
                                       'embedder', 'default_model')

    # Check if any of the affected files are relevant to the histogram check.
    relevant_files_changed = False
    for f in input_api.AffectedFiles():
        path = f.LocalPath()
        if path.startswith(model_dir) and path.endswith('.cc') and \
           not path.endswith(('_unittest.cc', '_test.cc')):
            relevant_files_changed = True
            break

    if not relevant_files_changed:
        return []

    # Run the full check if relevant files were changed.
    cwd = input_api.PresubmitLocalPath()
    try:
        old_sys_path = input_api.sys.path
        sys_path_to_add = input_api.os_path.join(cwd, 'tools')
        input_api.sys.path.insert(0, sys_path_to_add)
        import check_histograms
    finally:
        input_api.sys.path = old_sys_path

    expected_histograms = (check_histograms.GetExpectedHistogramsFileContent())
    actual_histograms = check_histograms.GetActualHistogramsFileContent()

    if expected_histograms != actual_histograms:
        error_message = (
            'The histograms.txt file is out of date.'
            '\n\nPlease run:\n'
            'python3 components/segmentation_platform/tools/check_histograms.py'
        )
        return [output_api.PresubmitPromptWarning(error_message, [])]

    return []


def _CommonChecks(input_api, output_api):
    output = GetPylintConfiguration(input_api, output_api)
    cwd = input_api.PresubmitLocalPath()
    tool_help_path = GetToolPathRelativeToRepositoryRoot(input_api)

    filter_file_data = FetchLauncherFilterFileData(input_api, cwd)

    if filter_file_data['expected'] != filter_file_data['actual']:
        output.append(
            output_api.PresubmitPromptWarning(
                'The test launcher filter file does not match the ' +
                f'available tests.\n\nPlease run:\n{tool_help_path}', []))

    output.extend(_CheckHistograms(input_api, output_api))
    return output


def CheckChangeOnUpload(*args):
    return _CommonChecks(*args)


def CheckChangeOnCommit(*args):
    return _CommonChecks(*args)
