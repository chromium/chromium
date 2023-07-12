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

from signing import commands, invoker, logger, model

_LOG_FILE_URL = 'LogFileURL'

_NOTARY_SERVICE_MAX_RETRIES = 3


class NotarizationError(Exception):
    pass


class Invoker(invoker.Base):

    @staticmethod
    def register_arguments(parser):
        parser.add_argument(
            '--notary-user',
            help='The username used to authenticate to the Apple notary '
            'service.')
        parser.add_argument(
            '--notary-password',
            help='The password or password reference (e.g. @keychain, see '
            '`xcrun altool -h`) used to authenticate to the Apple notary '
            'service.')
        parser.add_argument(
            '--notary-team-id',
            help='The Apple Developer Team ID used to authenticate to the '
            'Apple notary service.')

    def __init__(self, args, config):
        self._notary_user = args.notary_user
        self._notary_password = args.notary_password
        # Let the config specify the notary_team_id if one was not passed as an
        # argument. This may be None.
        self._notary_team_id = args.notary_team_id or config.notary_team_id

        if not config.notarize.should_notarize():
            return

        if not self._notary_team_id:
            raise invoker.InvokerConfigError(
                'Notarization requires a `--notary-team-id`.')
        if not self._notary_user or not self._notary_password:
            raise invoker.InvokerConfigError(
                'The `--notary-user` and `--notary-password` '
                'arguments are required if notarizing.')

    def submit(self, path, config):
        command = [
            'xcrun',
            'notarytool',
            'submit',
            path,
            '--apple-id',
            self._notary_user,
            '--team-id',
            self._notary_team_id,
            '--no-wait',
            '--output-format',
            'plist',
        ]

        # TODO(rsesek): As the reliability of notarytool is determined,
        # potentially run the command through _notary_service_retry().

        output = commands.run_password_command_output(
            command, _altool_password_for_notarytool(self._notary_password))
        try:
            plist = plistlib.loads(output)
            return plist['id']
        except:
            raise NotarizationError(
                'xcrun notarytool returned output that could not be parsed: {}'
                .format(output))

    def get_result(self, uuid, config):
        command = [
            'xcrun',
            'notarytool',
            'info',
            uuid,
            '--apple-id',
            self._notary_user,
            '--team-id',
            self._notary_team_id,
            '--output-format',
            'plist',
        ]
        output = commands.run_password_command_output(
            command, _altool_password_for_notarytool(self._notary_password))

        plist = plistlib.loads(output)
        status = plist['status']
        if status == 'In Progress':
            return NotarizationResult(Status.IN_PROGRESS, status, output, None)
        if status == 'Accepted':
            return NotarizationResult(Status.SUCCESS, status, output, None)
        # notarytool does not provide log file URLs, so instead try to fetch
        # the log on failure.
        try:
            log = self._get_log(uuid, config).decode('utf8')
        except Exception as e:
            logger.error('Failed to get the notarization log data', e)
            log = None
        return NotarizationResult(Status.ERROR, status, output, log)

    def _get_log(self, uuid, config):
        command = [
            'xcrun', 'notarytool', 'log', uuid, '--apple-id', self._notary_user,
            '--team-id', self._notary_team_id
        ]
        return commands.run_password_command_output(
            command, _altool_password_for_notarytool(self._notary_password))


def submit(path, config):
    """Submits an artifact to Apple for notarization.

    Args:
        path: The path to the artifact that will be uploaded for notarization.
        config: The |config.CodeSignConfig| for the artifact.

    Returns:
        A UUID from the notary service that represents the request.
    """
    uuid = config.invoker.notarizer.submit(path, config)
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
            result = config.invoker.notarizer.get_result(uuid, config)
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
    _notary_service_retry(
        staple_command, (65, 68), 'staple', sleep_before_retry=True)


def _notary_service_retry(func,
                          known_bad_returncodes,
                          short_command_name,
                          sleep_before_retry=False):
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
        sleep_before_retry: If True, will wait before re-trying when a known
            failure occurs.

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
                if sleep_before_retry:
                    time.sleep(30)
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
