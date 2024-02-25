#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Resets DUT for testing.

Resets DUT states before testing. This script provides a quick way to reset
DUT to a state that there is no install attributes, no previous oobe state
files, and no stub policy blob files. So that tests that exercise the login
code paths could pass login.

Note TPM is not reset so tests that deal with ownership or enrollment might
not work.

The logic is referenced from `tast-tests`:
    src/go.chromium.org/tast-tests/cros/common/hwsec/helpers.go
"""

import argparse
import logging
import os
from pathlib import Path
from typing import Dict, List, Optional
import subprocess
import sys

SYSTEM_STATE_FILES = [
    "/home/.shadow",
    "/home/chronos/.oobe_completed",
    "/home/chronos/Local State",
    "/mnt/stateful_partition/.tpm_owned",
    "/run/cryptohome",
    "/run/lockbox/install_attributes.pb",
    "/run/tpm_manager",
    "/var/cache/app_pack",
    "/var/cache/shill/default.profile",
    "/var/lib/bootlockbox",
    "/var/lib/chaps",
    "/var/lib/cryptohome",
    "/var/lib/device_management",
    "/var/lib/oobe_config_restore",
    "/var/lib/oobe_config_save",
    "/var/lib/public_mount_salt",
    "/var/lib/tpm_manager",
    "/var/lib/tpm",
    "/var/lib/u2f",
]

SYSTEM_STATE_GLOBS = ["/var/lib/devicesettings/*"]

DAEMONS = [
    # High level TPM daemons
    "tpm_managerd",
    "chapsd",
    "bootlockboxd",
    "pca_agentd",
    "attestationd",
    "u2fd",
    "cryptohomed",
    "device_managementd",
    # Stateful daemons
    "update-engine",
    # Biod
    "biod",
    # Need to stop to release dmcrypt mounts for `lvremove`.
    "vm_concierge",
]


def _read_release_file(path: Path) -> Dict[str, str]:
    return dict([(x.strip() for x in line.split("=", 1))
                 for line in path.read_text(encoding="utf-8").splitlines()])


def _is_chromeos() -> bool:
    os_release = _read_release_file(Path("/etc/os-release"))
    return os_release.get("ID") in ["chromeos", "chromiumos"]


def _run_cmd(args: List[str], shell=False):
    logging.info("Run: %s", args)
    process = subprocess.run(args,
                             shell=shell,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    logging.info("Output: %s", process.stdout[:80])


def reset_system_state_files():
    """Resets a known list of system state files and restart daemons"""

    # Clean-ups before resetting states.
    _run_cmd(["cryptohome", "--action=pkcs11_terminate"])
    _run_cmd(["cryptohome", "--action=unmount"])
    _run_cmd(["umount", "/run/namespaces/mnt_chrome"])
    _run_cmd(["process_killer", "--session", "--mount_holders"])

    # Stops daemons that touch the system state files. Note the script does
    # not touch `ui` service because it runs as part of
    # chromeos_integration_tests which stop ui service while running.
    for d in DAEMONS:
        logging.info("Stopping %s.", d)
        _run_cmd(["initctl", "stop", d])

    logging.info("Removing state files.")
    _run_cmd(["rm", "-rf", "--"] + SYSTEM_STATE_FILES)
    _run_cmd(["bash", "-c", " ".join(["rm", "-rf"] + SYSTEM_STATE_GLOBS)])

    logging.info("Update LVM.")
    _run_cmd(["vgchange", "-ay"])
    # Run with shell so that globing in `"/dev/*/cryptohome*"` works.
    _run_cmd(["lvremove -ff /dev/*/cryptohome*"], shell=True)

    logging.info("Run tmpfiles to restore the removed folders and permissions.")
    _run_cmd([
        "/usr/bin/systemd-tmpfiles", "--create", "--remove", "--boot",
        "--prefix", "/home", "--prefix", "/var/lib"
    ])

    for d in DAEMONS:
        logging.info("Starting %s.", d)
        _run_cmd(["initctl", "start", d])


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    """Main function"""
    assert _is_chromeos(), "This script only runs on ChromeOS DUT."

    parser = argparse.ArgumentParser(description=__doc__)
    opts = parser.parse_args(argv)

    logging.basicConfig(level=logging.INFO)
    return reset_system_state_files()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
