# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs commands on the sender DUT, either over SSH or on this machine.

Use make_transport() to get the right Transport for a sender hostname. The
rest of the perf test code should not need to know whether the sender is local.
"""

import abc
import logging
import os
import shutil
import socket
import subprocess
import time

import perf_config

LOCAL_SENDERS = ('localhost', '127.0.0.1', None)
SSH_PORT = 22
SSH_KEY_PATH = os.path.expanduser('~/.ssh/id_ed25519')
# BatchMode makes ssh fail instead of hanging on a password/passphrase prompt,
# and ConnectTimeout bounds how long ssh waits for an unreachable host.
SSH_BASE_OPTS = [
    '-o',
    'StrictHostKeyChecking=no',
    '-o',
    'BatchMode=yes',
    '-o',
    'ConnectTimeout=10',
]
# ssh exits with 255 when it fails to connect or authenticate. Any other code
# is the exit status of the remote command.
SSH_CONNECT_ERROR = 255
COMMAND_TIMEOUT_SECS = 120
COPY_TIMEOUT_SECS = 30

PREFLIGHT_TCP_TIMEOUT_SECS = 5
PREFLIGHT_MAX_ATTEMPTS = 3
PREFLIGHT_RETRY_DELAY_SECS = 5
INFRA_FAILURE_NOTE = 'This is an infra failure, not a test failure.'

# Maps (host, username) -> None on success, or the RemoteDeviceError that
# made the connectivity check fail. Keeps the check to once per process.
_preflight_results = {}


class RemoteDeviceError(RuntimeError):
    """The remote sender DUT is unusable.

    This is an infra failure, not a test failure.
    """

    def __init__(self, message, reason=None):
        super().__init__(message)
        # Short cause (e.g. "[Errno 113] No route to host") for retry logs.
        self.reason = reason or message


class SenderNotFoundError(RemoteDeviceError):
    """The sender hostname does not resolve in DNS.

    This is an infra failure, not a test failure.
    """


class SenderUnreachableError(RemoteDeviceError):
    """The sender cannot be reached over the network on the SSH port.

    This is an infra failure, not a test failure.
    """


class SenderSshError(RemoteDeviceError):
    """The SSH port is reachable, but running a command over SSH fails.

    This is an infra failure, not a test failure.
    """


class Transport(abc.ABC):
    """Runs shell commands on the sender."""

    def __init__(self, host, username):
        self.host = host
        self.username = username

    @property
    @abc.abstractmethod
    def is_local(self):
        """True if commands run on this machine rather than over SSH."""

    @abc.abstractmethod
    def run(self, command, timeout=COMMAND_TIMEOUT_SECS):
        """Runs `command` to completion.

        Returns:
            subprocess.CompletedProcess with text stdout and stderr.
        """

    @abc.abstractmethod
    def spawn(self, command):
        """Starts `command` without waiting for it.

        Returns:
            subprocess.Popen with piped stdin, stdout, and stderr.
        """

    @abc.abstractmethod
    def copy_from(self, remote_path, local_path, timeout=COPY_TIMEOUT_SECS):
        """Copies `remote_path` on the sender to `local_path` on this host."""

    def verify_connectivity(self):
        """Raises RemoteDeviceError if the sender cannot run commands."""

    @property
    def known_unreachable(self):
        """True if an earlier check found the sender unreachable."""
        return False

    def raise_if_disconnected(self, result, action):
        """Raises SenderUnreachableError if `result` is an ssh failure."""

    def open_tunnel(self, local_port, remote_port):
        """Forwards `local_port` to the sender and `remote_port` back.

        Returns:
            The tunnel's subprocess.Popen, or None if no tunnel is needed.
        """
        del local_port, remote_port


class LocalTransport(Transport):
    """Runs commands on this machine."""

    @property
    def is_local(self):
        return True

    def run(self, command, timeout=COMMAND_TIMEOUT_SECS):
        logging.debug('Executing local command: %s', command)
        return subprocess.run(
            command,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )

    def spawn(self, command):
        logging.debug('Executing local command: %s', command)
        return subprocess.Popen(  # pylint: disable=consider-using-with
            command,
            shell=True,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def copy_from(self, remote_path, local_path, timeout=COPY_TIMEOUT_SECS):
        del timeout
        if os.path.exists(remote_path):
            shutil.copy(remote_path, local_path)

    def open_tunnel(self, local_port, remote_port):
        logging.info("Local sender detected. Skipping SSH tunnel.")
        return super().open_tunnel(local_port, remote_port)


class SshTransport(Transport):
    """Runs commands on the sender over SSH."""

    @property
    def is_local(self):
        return False

    @property
    def _destination(self):
        return f'{self.username}@{self.host}'

    def _ssh_argv(self, command):
        return [
            'ssh',
            *SSH_BASE_OPTS,
            '-i',
            SSH_KEY_PATH,
            self._destination,
            command,
        ]

    def run(self, command, timeout=COMMAND_TIMEOUT_SECS):
        argv = self._ssh_argv(command)
        logging.debug('Executing SSH command: %s', ' '.join(argv))
        result = subprocess.run(
            argv, capture_output=True, text=True, timeout=timeout, check=False
        )
        if result.returncode == SSH_CONNECT_ERROR:
            logging.error(
                'SSH to %s failed (rc=%d): %s',
                self.host,
                result.returncode,
                (result.stderr or '').strip(),
            )
        return result

    def spawn(self, command):
        argv = self._ssh_argv(command)
        logging.debug('Executing SSH command: %s', ' '.join(argv))
        return subprocess.Popen(  # pylint: disable=consider-using-with
            argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def copy_from(self, remote_path, local_path, timeout=COPY_TIMEOUT_SECS):
        subprocess.run(
            [
                'scp',
                '-i',
                SSH_KEY_PATH,
                *SSH_BASE_OPTS,
                f'{self._destination}:{remote_path}',
                local_path,
            ],
            check=False,
            timeout=timeout,
        )

    def open_tunnel(self, local_port, remote_port):
        self.verify_connectivity()
        tunnel_argv = [
            'ssh',
            '-i',
            SSH_KEY_PATH,
            # Optimization for tunnel throughput. Disable compression as video
            # data is already compressed.
            '-o',
            'Compression=no',
            '-o',
            'ServerAliveInterval=10',
            '-o',
            'ServerAliveCountMax=10',
            '-o',
            'TCPKeepAlive=yes',
            '-o',
            'ExitOnForwardFailure=yes',
            '-L',
            f'{local_port}:{perf_config.LOCAL_HOST_IP}:{local_port}',
            '-R',
            f'{remote_port}:{perf_config.LOCAL_HOST_IP}:{remote_port}',
            self._destination,
            '-N',
        ]
        # pylint: disable-next=consider-using-with
        tunnel = subprocess.Popen(tunnel_argv)
        logging.info("Started tunnel.")
        return tunnel

    # --- Connectivity ---

    @property
    def _cache_key(self):
        return (self.host, self.username)

    @property
    def known_unreachable(self):
        return _preflight_results.get(self._cache_key) is not None

    def _mark_unreachable(self, error):
        _preflight_results[self._cache_key] = error

    def raise_if_disconnected(self, result, action):
        if result.returncode != SSH_CONNECT_ERROR:
            return
        stderr = (result.stderr or '').strip() or '<empty>'
        error = SenderUnreachableError(
            f"Lost SSH connection to sender '{self.host}' while {action} "
            f"(rc={result.returncode}): {stderr}. {INFRA_FAILURE_NOTE}",
            stderr,
        )
        self._mark_unreachable(error)
        raise error

    def verify_connectivity(self):
        """Fails fast if the sender is missing or unreachable.

        Checks, in order, that the host resolves in DNS, accepts TCP
        connections on port 22, and can run a trivial command over SSH. The
        sequence is retried so that a briefly rebooting DUT does not fail the
        run. The outcome is cached per (host, username) for the process.

        Raises:
            SenderNotFoundError: The hostname does not resolve.
            SenderUnreachableError: TCP port 22 cannot be reached.
            SenderSshError: SSH connects but the test command fails.
        """
        if self._cache_key in _preflight_results:
            cached_error = _preflight_results[self._cache_key]
            if cached_error is not None:
                raise cached_error
            return

        last_error = None
        for attempt in range(1, PREFLIGHT_MAX_ATTEMPTS + 1):
            try:
                ip = self._check_dns()
                self._check_tcp(ip)
                self._check_ssh()
            except RemoteDeviceError as e:
                last_error = e
                logging.warning(
                    'Sender check attempt %d/%d failed for %s: %s',
                    attempt,
                    PREFLIGHT_MAX_ATTEMPTS,
                    self.host,
                    e.reason,
                )
                if attempt < PREFLIGHT_MAX_ATTEMPTS:
                    time.sleep(PREFLIGHT_RETRY_DELAY_SECS)
                continue

            logging.info('Sender %s (%s) reachable over SSH.', self.host, ip)
            _preflight_results[self._cache_key] = None
            return

        self._mark_unreachable(last_error)
        raise last_error

    def _check_dns(self):
        """Resolves the host and returns its first IP address."""
        try:
            addrinfo = socket.getaddrinfo(self.host, SSH_PORT)
        except socket.gaierror as e:
            reason = f'DNS lookup failed: {e}'
            raise SenderNotFoundError(
                f"Sender '{self.host}' does not resolve ({reason}). Check the "
                f"--sender hostname and the lab DHCP/DNS entry. "
                f"{INFRA_FAILURE_NOTE}",
                reason,
            ) from e
        # Each entry is (family, type, proto, canonname, sockaddr); sockaddr[0]
        # is the IP address for both IPv4 and IPv6.
        return addrinfo[0][4][0]

    def _check_tcp(self, ip):
        """Opens and immediately closes a TCP connection to port 22."""
        try:
            conn = socket.create_connection(
                (self.host, SSH_PORT), timeout=PREFLIGHT_TCP_TIMEOUT_SECS
            )
        except OSError as e:
            # socket.timeout has no errno/strerror, so fall back to str(e).
            if e.errno is not None:
                reason = f'[Errno {e.errno}] {e.strerror or e}'
            else:
                reason = str(e) or type(e).__name__
            raise SenderUnreachableError(
                f"Sender '{self.host}' ({ip}) is not reachable on port "
                f"{SSH_PORT}: {reason}. Likely causes: device powered "
                "off/asleep/rebooting, network link or DHCP lease lost, or "
                f"sshd not running. {INFRA_FAILURE_NOTE}",
                reason,
            ) from e
        conn.close()

    def _check_ssh(self):
        """Runs a trivial command over SSH and verifies its output."""
        # `echo ok` behaves the same in cmd, PowerShell, and POSIX shells.
        try:
            result = self.run('echo ok')
        except subprocess.TimeoutExpired as e:
            reason = f"ssh timed out after {e.timeout}s running 'echo ok'"
            raise SenderSshError(
                f"SSH to {self._destination} failed: {reason}. "
                f"{INFRA_FAILURE_NOTE}",
                reason,
            ) from e

        stdout = (result.stdout or '').strip()
        if result.returncode != 0 or stdout != 'ok':
            stderr = (result.stderr or '').strip() or '<empty>'
            reason = f'rc={result.returncode}, stdout={stdout!r}: {stderr}'
            raise SenderSshError(
                f"SSH to {self._destination} failed (rc={result.returncode}, "
                f"stdout={stdout!r}). Check the SSH key, authorized_keys, and "
                f"host key on the sender. {INFRA_FAILURE_NOTE}\n"
                f"ssh stderr:\n{stderr}",
                reason,
            )


def make_transport(host, username):
    """Returns a LocalTransport for local senders, else an SshTransport."""
    if host in LOCAL_SENDERS:
        return LocalTransport(host, username)
    return SshTransport(host, username)
