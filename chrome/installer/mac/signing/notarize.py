# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The notarization module manages uploading artifacts for notarization, polling
for results, and stapling Apple Notary notarization tickets.
"""

import collections
import enum
import os
import plistlib
import subprocess
import time

from . import commands, logger, model

_LOG_FILE_URL = 'LogFileURL'

_NOTARY_SERVICE_MAX_RETRIES = 3


class NotarizationError(Exception):
    pass


def _get_notarizaton_tool_cmd(config):
    if config.notarization_tool_path:
        return [config.notarization_tool_path]
    return ['xcrun', str(config.notarization_tool)]


def _submit_altool(path, config):
    assert config.notarization_tool == model.NotarizationTool.ALTOOL
    command = _get_notarizaton_tool_cmd(config) + [
        '--notarize-app', '--file', path, '--primary-bundle-id',
        config.base_bundle_id, '--username', config.notary_user, '--password',
        config.notary_password, '--output-format', 'xml'
    ]
    if config.notary_asc_provider is not None:
        command.extend(['--asc-provider', config.notary_asc_provider])

    def submit_comand():
        return commands.run_command_output(command)

    # Known bad codes:
    # 1 - Xcode 12 altool does not always exit with distinct error codes, so
    #     this is general failure.
    # 13 - A server with the specified hostname could not be found.
    # 176 - Unable to find requested file(s): metadata.xml (1057)
    # 236 - Exception occurred when creating MZContentProviderUpload for
    #       provider. (1004)
    # 240 - SIGSEGV in the Java Runtime Environment
    # 250 - Unable to process upload done request at this time due to a general
    #       error (1018)
    output = _notary_service_retry(submit_comand, (1, 13, 176, 236, 240, 250),
                                   'submission')

    try:
        plist = plistlib.loads(output)
        return plist['notarization-upload']['RequestUUID']
    except:
        raise NotarizationError(
            'xcrun altool returned output that could not be parsed: {}'.format(
                output))


def _submit_notarytool(path, config):
    assert config.notarization_tool == model.NotarizationTool.NOTARYTOOL
    command = _get_notarizaton_tool_cmd(config) + [
        'submit',
        path,
        '--apple-id',
        config.notary_user,
        '--team-id',
        config.notary_team_id,
        '--no-wait',
        '--output-format',
        'plist',
    ]

    # TODO(rsesek): As the reliability of notarytool is determined, potentially
    # run the command through _notary_service_retry().

    output = commands.run_password_command_output(
        command, _altool_password_for_notarytool(config.notary_password))
    try:
        plist = plistlib.loads(output)
        return plist['id']
    except:
        raise NotarizationError(
            'xcrun notarytool returned output that could not be parsed: {}'
            .format(output))


def submit(path, config):
    """Submits an artifact to Apple for notarization.

    Args:
        path: The path to the artifact that will be uploaded for notarization.
        config: The |config.CodeSignConfig| for the artifact.

    Returns:
        A UUID from the notary service that represents the request.
    """
    do_submit = {
        model.NotarizationTool.ALTOOL: _submit_altool,
        model.NotarizationTool.NOTARYTOOL: _submit_notarytool,
    }[config.notarization_tool]
    uuid = do_submit(path, config)
    logger.info('Submitted %s for notarization, request UUID: %s.', path, uuid)
    return uuid


class Status(enum.Enum):
    """Enum representing the state of a notarization request."""
    SUCCESS = enum.auto()
    IN_PROGRESS = enum.auto()
    ERROR = enum.auto()


"""Tuple object that contains the status and result information of a
notarization request.
"""
NotarizationResult = collections.namedtuple(
    'NotarizationResult', ['status', 'status_string', 'output', 'log_file'])


def _get_result_altool(uuid, config):
    assert config.notarization_tool == model.NotarizationTool.ALTOOL
    try:
        command = _get_notarizaton_tool_cmd(config) + [
            '--notarization-info', uuid, '--username', config.notary_user,
            '--password', config.notary_password, '--output-format', 'xml'
        ]
        if config.notary_asc_provider is not None:
            command.extend(['--asc-provider', config.notary_asc_provider])
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
                return NotarizationResult(Status.IN_PROGRESS, None, None, None)
        # Sometimes there are network hiccups when fetching notarization
        # info, but that often fixes itself and shouldn't derail the
        # entire signing operation. More serious extended connectivity
        # problems will eventually fall through to the "no results"
        # timeout.
        if e.returncode == 13:
            logger.warning(e.output)
            return NotarizationResult(Status.IN_PROGRESS, None, None, None)
        # And other times the command exits with code 1 and no further output.
        if e.returncode == 1:
            logger.warning(e.output)
            return NotarizationResult(Status.IN_PROGRESS, None, None, None)
        raise e

    plist = plistlib.loads(output)
    info = plist['notarization-info']
    status = info['Status']
    if status == 'in progress':
        return NotarizationResult(Status.IN_PROGRESS, status, output, None)
    if status == 'success':
        return NotarizationResult(Status.SUCCESS, status, output,
                                  info[_LOG_FILE_URL])
    return NotarizationResult(Status.ERROR, status, output, info[_LOG_FILE_URL])


def _get_log_notarytool(uuid, config):
    assert config.notarization_tool == model.NotarizationTool.NOTARYTOOL
    command = _get_notarizaton_tool_cmd(config) + [
        'log', uuid, '--apple-id', config.notary_user, '--team-id',
        config.notary_team_id
    ]
    return commands.run_password_command_output(
        command, _altool_password_for_notarytool(config.notary_password))


def _get_result_notarytool(uuid, config):
    assert config.notarization_tool == model.NotarizationTool.NOTARYTOOL
    command = _get_notarizaton_tool_cmd(config) + [
        'info',
        uuid,
        '--apple-id',
        config.notary_user,
        '--team-id',
        config.notary_team_id,
        '--output-format',
        'plist',
    ]
    output = commands.run_password_command_output(
        command, _altool_password_for_notarytool(config.notary_password))

    plist = plistlib.loads(output)
    status = plist['status']
    if status == 'In Progress':
        return NotarizationResult(Status.IN_PROGRESS, status, output, None)
    if status == 'Accepted':
        return NotarizationResult(Status.SUCCESS, status, output, None)
    # notarytool does not provide log file URLs, so instead try to fetch
    # the log on failure.
    try:
        log = _get_log_notarytool(uuid, config).decode('utf8')
    except Exception as e:
        logger.error('Failed to get the notarization log data', e)
        log = None
    return NotarizationResult(Status.ERROR, status, output, log)


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

    get_result = {
        model.NotarizationTool.ALTOOL: _get_result_altool,
        model.NotarizationTool.NOTARYTOOL: _get_result_notarytool,
    }[config.notarization_tool]

    while len(wait_set) > 0:
        for uuid in list(wait_set):
            result = get_result(uuid, config)
            if result.status == Status.IN_PROGRESS:
                continue
            elif result.status == Status.SUCCESS:
                logger.info('Successfully notarized request %s. Log file: %s',
                            uuid, result.log_file)
                wait_set.remove(uuid)
                yield uuid
            else:
                logger.error(
                    'Failed to notarize request %s.\n'
                    'Output:\n%s\n'
                    'Log file:\n%s', uuid, result.output, result.log_file)
                raise NotarizationError(
                    'Notarization request {} failed with status: "{}".'.format(
                        uuid,
                        result.status_string,
                    ))

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


def _altool_password_for_notarytool(password):
    """Parses an altool password value (e.g. "@env:PASSWORD_ENV_VAR") and
    returns it as a string. Only '@env' is supported.

    Args:
        password: A string passed to the `altool` executable as a password.
    Returns:
        The actual password.
    """
    at_env = '@env:'
    if password.startswith(at_env):
        password = os.getenv(password[len(at_env):])
    elif password.startswith('@keychain:'):
        raise ValueError('Unsupported notarytool password: ' + password)

    return password
