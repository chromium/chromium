# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The notarization module manages uploading artifacts for notarization, polling
for results, and stapling Apple Notary notarization tickets.
"""

import asyncio
import collections
import enum
import os
import plistlib
import subprocess
import time

from signing import commands, invoker, logger, model

_LOG_FILE_URL = 'LogFileURL'

_NOTARY_SERVICE_DEFAULT_MAX_TRIES = 3
_NOTARY_SERVICE_RETRY_DELAY = 30
_NOTARY_SERVICE_MAX_RETRY_DELAY = 120


class NotarizationError(Exception):
    pass


class Invoker(invoker.Base):

    @staticmethod
    def register_arguments(parser):
        parser.add_argument(
            '--notary-arg',
            action='append',
            default=[],
            help='Specifies additional arguments to pass to the notarization '
            'tool. If specified multiple times, the arguments are passed in '
            'the given order. These are passed to every invocation of the '
            'notarization tool and are intended to specify authentication '
            'parameters.')

    def __init__(self, args, config):
        self._notary_args = args.notary_arg

    @property
    def notary_args(self):
        return self._notary_args

    async def submit(self, path, config):
        # Submit the notarization.
        command = [
            'xcrun',
            'notarytool',
            'submit',
            path,
            '--no-wait',
            '--output-format',
            'plist',
        ] + self.notary_args
        output = await commands.run_command_output_async(command)
        try:
            uuid = plistlib.loads(output)['id']
        except:
            raise NotarizationError(
                'xcrun notarytool returned output that could not be parsed: {}'
                .format(output))
        logger.info('Submitted %s for notarization, request UUID: %s.', path,
                    uuid)

        # Wait for notarization to complete.
        while True:
            result = await self.get_result(uuid, config)
            if result.status == Status.IN_PROGRESS:
                await asyncio.sleep(5)
                continue
            elif result.status == Status.SUCCESS:
                logger.info('Successfully notarized request %s. Log file: %s',
                            uuid, result.log_file)
                return
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

    async def get_result(self, uuid, config):
        command = [
            'xcrun',
            'notarytool',
            'info',
            uuid,
            '--output-format',
            'plist',
        ] + self.notary_args
        output = await commands.run_command_output_async(command)

        plist = plistlib.loads(output)
        status = plist['status']
        if status == 'In Progress':
            return NotarizationResult(Status.IN_PROGRESS, status, output, None)
        if status == 'Accepted':
            return NotarizationResult(Status.SUCCESS, status, output, None)
        # notarytool does not provide log file URLs, so instead try to fetch
        # the log on failure.
        try:
            log = (await self._get_log(uuid, config)).decode('utf8')
        except Exception as e:
            logger.error('Failed to get the notarization log data', e)
            log = None
        return NotarizationResult(Status.ERROR, status, output, log)

    async def _get_log(self, uuid, config):
        command = ['xcrun', 'notarytool', 'log', uuid] + self.notary_args
        return await commands.run_command_output_async(command)


async def submit(path, config):
    """Submits an artifact to Apple for notarization and awaits its success.

    Args:
        path: The path to the artifact that will be uploaded for notarization.
        config: The |config.CodeSignConfig| for the artifact.
    """
    await config.invoker.notarizer.submit(path, config)


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

    retry = Retry('staple', sleep_before_retry=True)
    while retry.keep_going():
        try:
            commands.run_command(
                ['xcrun', 'stapler', 'staple', '--verbose', path])
            return
        except subprocess.CalledProcessError as e:
            # Known bad codes:
            bad_codes = (
                65,  # CloudKit query failed due to "(null)"
                68,  # A server with the specified hostname could not be found.
            )
            if e.returncode in bad_codes and retry.failed_should_retry(
                    f'Output: {e.output}'):
                continue
            raise e


class Retry(object):
    """Retry is a helper class that manages retrying notarization operations
    that may fail due to transient issues. Usage:

        retry = Retry('staple')
        while retry.keep_going():
            try:
                return operation()
            except Exception as e:
                if is_transient(e) and retry.failed_should_retry():
                    continue
                raise e
    """

    def __init__(self,
                 desc,
                 sleep_before_retry=False,
                 max_tries=_NOTARY_SERVICE_DEFAULT_MAX_TRIES):
        """Creates a retry state object.

        Args:
            desc: A short description of the operation to retry.
            sleep_before_retry: If True, will sleep before proceeding with
                a retry.
            max_tries: Max number of attempts.
        """
        self._attempt = 0
        self._desc = desc
        self._sleep_before_retry = sleep_before_retry
        self._max_tries = max_tries

    def keep_going(self):
        """Used as the condition for a retry loop."""
        if self._attempt < self._max_tries:
            return True
        raise RuntimeError(
            'Loop should have terminated at failed_should_retry()')

    def failed_should_retry(self, msg=''):
        """If the operation failed and the caller wants to retry it, this
        method increments the attempt count and determines if another
        attempt should be made.

        Args:
            msg: An optional message to describe the failure.

        Returns:
            True if the retry loop should continue, and False if the loop
            should terminate with an error.
        """
        should_retry = self._attempt < self._max_tries - 1
        if should_retry:
            delay = min((2**(self._attempt)) * _NOTARY_SERVICE_RETRY_DELAY,
                        _NOTARY_SERVICE_MAX_RETRY_DELAY)
            retry_when_message = (
                f'after {delay} second{"s" if delay != 1 else ""}'
                if self._sleep_before_retry else 'immediately')
            logger.warning(f'Error during notarization command {self._desc}. ' +
                           f'Retrying {retry_when_message}. {msg}')
            if self._sleep_before_retry:
                time.sleep(delay)
        self._attempt += 1
        return should_retry
