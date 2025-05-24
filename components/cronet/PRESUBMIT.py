# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for src/components/cronet.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

PRESUBMIT_VERSION = '2.0.0'


def CheckPyLint(input_api, output_api):
    pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api)
    return input_api.RunTests(pylint_checks)


def CheckPackage(input_api, output_api):
    """Verify API classes are in org.chromium.net package, and implementation
  classes are not in org.chromium.net package."""
    api_packages = ['org.chromium.net', 'org.chromium.net.apihelpers']
    api_packages_regex = '(' + '|'.join(api_packages) + ')'
    api_file_pattern = input_api.re.compile(
        r'^components/cronet/android/api/.*\.(java|template)$')
    impl_file_pattern = input_api.re.compile(
        r'^components/cronet/android/java/.*\.(java|template)$')
    invalid_api_package_pattern = input_api.re.compile(r'^package (?!' +
                                                       api_packages_regex +
                                                       ';)')
    invalid_impl_package_pattern = input_api.re.compile(r'^package ' +
                                                        api_packages_regex +
                                                        ';')

    source_filter = lambda path: input_api.FilterSourceFile(
        path,
        files_to_check=[r'^components/cronet/android/.*\.(java|template)$'])

    problems = []
    for f in input_api.AffectedSourceFiles(source_filter):
        local_path = f.LocalPath()
        for line_number, line in f.ChangedContents():
            if (api_file_pattern.search(local_path)):
                if (invalid_api_package_pattern.search(line)):
                    problems.append('%s:%d\n    %s' %
                                    (local_path, line_number, line.strip()))
            elif (impl_file_pattern.search(local_path)):
                if (invalid_impl_package_pattern.search(line)):
                    problems.append('%s:%d\n    %s' %
                                    (local_path, line_number, line.strip()))

    if problems:
        return [
            output_api.PresubmitError(
                'API classes must be in org.chromium.net package, '
                'and implementation\n'
                'classes must not be in org.chromium.net package.', problems)
        ]
    return []


def CheckUnittestsOnCommit(input_api, output_api):
    return input_api.RunTests(
        input_api.canned_checks.GetUnitTestsRecursively(
            input_api,
            output_api,
            os.path.join(input_api.change.RepositoryRoot(), 'components',
                         'cronet'),
            files_to_check=['.*test\\.py$'],
            files_to_skip=[]))


GOOD_CHANGE_ID_TXT = 'good_change_id'
BAD_CHANGE_ID_TXT = 'bad_change_id'
BUG_TXT = 'bugs'
COMMENT_TXT = 'comment'


def _GetBreakagesFilePathIfChanged(change):
    """ Returns |true| if the change may affect the breakages file. """

    for file in change.AffectedFiles(include_deletes=False):
        if file.LocalPath().endswith('breakages.json'):
            return file
    return None


def _IsValidChangeId(input_api, change_id):
    """ Returns |true| if the change_id is not valid.

  Validity means starting with the letter I followed by 40 hex chars.
  """
    if (input_api.re.fullmatch(r'^I[0-9a-fA-F]{40}$', change_id)
            and not input_api.re.fullmatch(r'^I00*$', change_id)):
        return True
    return False


def _GetInvalidChangeIdText(input_api, breakage, key):
    if key not in breakage:
        return ''
    if not _IsValidChangeId(input_api, breakage[key]):
        return '\t - entry has invalid %s: %s\n' % (key, breakage[key])
    return ''


def _GetMissingKeyText(breakage, key):
    if key in breakage:
        return ''
    return '\t - entry is missing the "%s" key\n' % key


def _GetGoodWithoutBadChangeIdText(breakage):
    if GOOD_CHANGE_ID_TXT in breakage and BAD_CHANGE_ID_TXT not in breakage:
        return '\t - entry cannot have %s without %s\n' % \
          (GOOD_CHANGE_ID_TXT, BAD_CHANGE_ID_TXT)
    return ''


def _GetUnknownKeyText(breakage):
    unknown_keys = []
    for key in breakage:
        if (key.startswith('_') or  # ignore comments
                key == BAD_CHANGE_ID_TXT or key == GOOD_CHANGE_ID_TXT or
                key == BUG_TXT or key == COMMENT_TXT):
            continue
        unknown_keys.append(key)

    if unknown_keys:
        return (f'\t - entry contains unknown key(s): {unknown_keys}. '
                f'Expected either {GOOD_CHANGE_ID_TXT}, {BUG_TXT} or '
                f'{COMMENT_TXT}\n')
    return ''


def CheckBreakagesFile(input_api, output_api):
    """Verify that the change_ids listed in the breakages file are valid."""
    breakages_file = _GetBreakagesFilePathIfChanged(input_api.change)
    if not breakages_file:
        return []
    breakages = input_api.json.loads(
        input_api.ReadFile(breakages_file))["breakages"]
    problems = []
    for i, breakage in enumerate(breakages):
        problem = ""
        # ensures that the entries, where existing are valid and that there are
        # no unknown keys.
        problem += _GetInvalidChangeIdText(input_api, breakage,
                                           BAD_CHANGE_ID_TXT)
        problem += _GetInvalidChangeIdText(input_api, breakage,
                                           GOOD_CHANGE_ID_TXT)
        problem += _GetGoodWithoutBadChangeIdText(breakage)
        problem += _GetMissingKeyText(breakage, BUG_TXT)
        problem += _GetUnknownKeyText(breakage)

        if problem:
            problems.append('Breakage Entry %d: \n%s' % (i, problem))

    if problems:
        return [
            output_api.PresubmitError(
                'The breakages.json file contains invalid entries.\n'
                'Please cross-check the entries.', problems)
        ]
    return []
