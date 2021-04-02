# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The notarization module manages uploading artifacts for notarization, polling
for results, and stapling Apple Notary notarization tickets.
"""

import os
import plistlib
import subprocess
import time

from . import commands, logger

# python2 support.
if not hasattr(plistlib, 'loads'):
    plistlib.loads = lambda s: plistlib.readPlistFromString(str(s))

_LOG_FILE_URL = 'LogFileURL'

_NOTARY_SERVICE_MAX_RETRIES = 3


class NotarizationError(Exception):
    pass


def submit(path, config):
    """Submits an artifact to Apple for notarization.

    Args:
        path: The path to the artifact that will be uploaded for notarization.
        config: The |config.CodeSignConfig| for the artifact.

    Returns:
        A UUID from the notary service that represents the request.
    """
    command = [
        'xcrun', 'altool', '--notarize-app', '--file', path,
        '--primary-bundle-id', config.base_bundle_id, '--username',
        config.notary_user, '--password', config.notary_password,
        '--output-format', 'xml'
    ]
    if config.notary_asc_provider is not None:
        command.extend(['--asc-provider', config.notary_asc_provider])

    def submit_comand():
        return commands.run_command_output(command)

    # Known bad codes:
    # 13 - A server with the specified hostname could not be found.
    # 176 - Unable to find requested file(s): metadata.xml (1057)
    # 236 - Exception occurred when creating MZContentProviderUpload for
    #       provider. (1004)
    # 240 - SIGSEGV in the Java Runtime Environment
    # 250 - Unable to process upload done request at this time due to a general
    #       error (1018)
    output = _notary_service_retry(submit_comand, (13, 176, 236, 240, 250),
                                   'submission')

    try:
        plist = plistlib.loads(output)
        uuid = plist['notarization-upload']['RequestUUID']
        logger.info('Submitted %s for notarization, request UUID: %s.', path,
                    uuid)
        return uuid
    except:
        raise NotarizationError(
            'xcrun altool returned output that could not be parsed: {}'.format(
                output))


def wait_for_results(uuids, config):
    """Waits for results from the notarization service. This iterates the list
    of UUIDs and checks the status of each one. For each successful result, the
    function yields to the caller. If a request failed, this raises a
    NotarizationError. If no requests are ready, this operation blocks and
    retries until a result is ready. After a certain amount of time, the
    operation will time out with a NotarizationError if no results are
    produced.

    Args:
        uuids: List of UUIDs to check for results. The list must not be empty.
        config: The |config.CodeSignConfig| object.

    Yields:
        The UUID of a successful notarization request.
    """
    assert len(uuids)

    wait_set = set(uuids)

    sleep_time_seconds = 5
    total_sleep_time_seconds = 0

    while len(wait_set) > 0:
        for uuid in list(wait_set):
            try:
                command = [
                    'xcrun', 'altool', '--notarization-info', uuid,
                    '--username', config.notary_user, '--password',
                    config.notary_password, '--output-format', 'xml'
                ]
                if config.notary_asc_provider is not None:
                    command.extend(
                        ['--asc-provider', config.notary_asc_provider])
                output = commands.run_command_output(command)
            except subprocess.CalledProcessError as e:
                # A notarization request might report as "not found" immediately
                # after submission, which causes altool to exit non-zero. Check
                # for this case and parse the XML output to ensure that the
                # error code refers to the not-found state. The UUID is known-
                # good since it was a result of submit(), so loop to wait for
                # it to show up.
                if e.returncode == 239:
                    plist = plistlib.loads(e.output)
                    if plist['product-errors'][0]['code'] == 1519:
                        continue
                # Sometimes there are network hiccups when fetching notarization
                # info, but that often fixes itself and shouldn't derail the
                # entire signing operation. More serious extended connectivity
                # problems will eventually fall through to the "no results"
                # timeout.
                if e.returncode == 13:
                    logger.warning(e.output)
                    continue
                raise e

            plist = plistlib.loads(output)
            info = plist['notarization-info']
            status = info['Status']
            if status == 'in progress':
                continue
            elif status == 'success':
                logger.info('Successfully notarized request %s. Log file: %s',
                            uuid, info[_LOG_FILE_URL])
                wait_set.remove(uuid)
                yield uuid
            else:
                logger.error(
                    'Failed to notarize request %s. Log file: %s. Output:\n%s',
                    uuid, info[_LOG_FILE_URL], output)
                raise NotarizationError(
                    'Notarization request {} failed with status: "{}". '
                    'Log file: {}.'.format(uuid, status, info[_LOG_FILE_URL]))

        if len(wait_set) > 0:
            # Do not wait more than 60 minutes for all the operations to
            # complete.
            if total_sleep_time_seconds < 60 * 60:
                # No results were available, so wait and try again in some
                # number of seconds. Do not wait more than 1 minute for any
                # iteration.
                time.sleep(sleep_time_seconds)
                total_sleep_time_seconds += sleep_time_seconds
                sleep_time_seconds = min(sleep_time_seconds * 2, 60)
            else:
                raise NotarizationError(
                    'Timed out waiting for notarization requests: {}'.format(
                        wait_set))


def staple_bundled_parts(parts, paths):
    """Staples all the bundled executable components of the app bundle.

    Args:
        parts: A list of |model.CodeSignedProduct|.
        paths: A |model.Paths| object.
    """
    # Only staple the signed, bundled executables.
    part_paths = [
        part.path
        for part in parts
        if part.path[-4:] in ('.app', '.xpc')
    ]
    # Reverse-sort the paths so that more nested paths are stapled before
    # less-nested ones.
    part_paths.sort(reverse=True)
    for part_path in part_paths:
        staple(os.path.join(paths.work, part_path))


def staple(path):
    """Staples a notarization ticket to the artifact at |path|. The
    notarization must have been performed with submit() and then completed by
    Apple's notary service, which can be checked with wait_for_one_result().

    Args:
        path: The path to the artifact that had previously been submitted for
            notarization and is now ready for stapling.
    """

    def staple_command():
        commands.run_command(['xcrun', 'stapler', 'staple', '--verbose', path])

    # Known bad codes:
    # 65 - CloudKit query failed due to "(null)"
    # 68 - A server with the specified hostname could not be found.
    _notary_service_retry(staple_command, (65, 68), 'staple')


def _notary_service_retry(func, known_bad_returncodes, short_command_name):
    """Calls the function |func| that runs a subprocess command, retrying it if
    the command exits uncleanly and the returncode is known to be bad (e.g.
    flaky).

    Args:
        func: The function to call within try block that wil catch
            CalledProcessError.
        known_bad_returncodes: An iterable of the returncodes that should be
            ignored and |func| retried.
        short_command_name: A short descriptive string of |func| that will be
            logged when |func| is retried.

    Returns:
        The result of |func|.
    """
    attempt = 0
    while True:
        try:
            return func()
        except subprocess.CalledProcessError as e:
            attempt += 1
            if (attempt < _NOTARY_SERVICE_MAX_RETRIES and
                    e.returncode in known_bad_returncodes):
                logger.warning('Retrying %s, exited %d, output: %s',
                               short_command_name, e.returncode, e.output)
            else:
                raise e
