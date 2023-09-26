#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs a socket server to run root tasks for chromeos_integration_tests.

chromeos_integration_tests needs to run as user "chronos" on ChromeOS devices
to simulate production chrome. As a result, it could not run root privileged
tasks such as clearing cryptohome vaults, or starting/stopping system daemons
that are required by some test cases. This helper script runs as user "root"
and provide a socket server to run these root privileged tasks on demand for
such test cases.

Simple protocol:
  Client requests are JSON strings with null terminators at the end.
    |<arbitrary command encoded as JSON>|0x00|

  Server responses are composed of 1 byte return code of the command and
  the output of the command with a null terminator at the end.
    |<1 byte return code>|<arbitrary output>|0x00|
"""

import argparse
import json
import logging
import os
from pathlib import Path
import socket
import subprocess
import sys
from typing import Dict, List, Optional


def _read_release_file(path: Path) -> Dict[str, str]:
    return dict([(x.strip() for x in line.split("=", 1))
                 for line in path.read_text(encoding="utf-8").splitlines()])


def _is_chromeos() -> bool:
    os_release = _read_release_file(Path("/etc/os-release"))
    return os_release.get("ID") in ["chromeos", "chromiumos"]


def _read_string(sock: socket.socket) -> str:
    """Reads a null terminated string from the given socket."""
    received = bytes()
    while True:
        buf = sock.recv(1024)
        if not buf:
            break

        received += buf
        if buf[-1] == 0:
            received = received[:-1]  # Strip null terminator.
            break

    return received.decode("utf-8")


def _send_string(sock: socket.socket, message: str):
    """Sends a string with a null terminator appended to the given socket."""
    buf = message.encode("utf-8")
    buf += b"\x00"
    sock.sendall(buf)


def _run_cmd(sock: socket.socket, cmd: str):
    """Runs the given command.

  Sends output and exit code to the given socket.
  """
    logging.info("Running : %s", cmd)
    try:
        process = subprocess.run(cmd,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT,
                                 shell=True,
                                 check=False)

        logging.info("Return code: %d", process.returncode)
        logging.info("Output: %s:", process.stdout[:80])

        sock.sendall(process.returncode.to_bytes(1, byteorder="big"))

        output = process.stdout + b"\x00"
        sock.sendall(output)

    except Exception as e:
        logging.error("Exception: %s", e)

        sock.sendall(b"\xFF")
        _send_string(sock, str(e))


class HelperServer:
    """Serves requests to run `root` privileged tasks."""
    def __init__(self, socket_path: str):
        self._socket_path = socket_path
        self._socket = None

    def _create_and_bind_socket(self):
        # `unlink` in case there was left over from previous runs.
        try:
            os.unlink(self._socket_path)
        except OSError:
            if os.path.exists(self._socket_path):
                raise

        self._socket = socket.socket(socket.AF_UNIX,
                                     socket.SOCK_STREAM | socket.SOCK_CLOEXEC)
        self._socket.bind(self._socket_path)

        # Allow access from all.
        os.chmod(self._socket_path, 0o777)

    def _handle_client(self, client_sock: socket.socket):
        """Handles the requests from a client."""
        request = json.loads(_read_string(client_sock))

        method = request["method"]

        if method == "runCommand":
            _run_cmd(client_sock, request["command"])
        else:
            logging.error("Unknown method: %s", method)

            client_sock.sendall(b"\xFF")
            _send_string(client_sock, ("Unknown method: %s", method))

        client_sock.close()

    def run(self) -> int:
        """Listens and processes client requests."""
        self._create_and_bind_socket()

        # Use 1 for pending connection queue since there should be only 1
        # client.
        self._socket.listen(1)

        logging.info("TestSudoHelperServer is listening at %s",
                     self._socket_path)

        while True:
            client_sock, client_address = self._socket.accept()
            logging.info("Connection from %s", client_address)

            self._handle_client(client_sock)

        return 0


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    """Main function for test_sudo_helper server."""
    assert _is_chromeos(), "This script only runs on ChromeOS DUT."

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--socket-path",
        required=True,
        help="The socket path where the server is listening.",
    )
    opts = parser.parse_args(argv)

    logging.basicConfig(level=logging.INFO)

    HelperServer(opts.socket_path).run()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
