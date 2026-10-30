#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import fcntl
import logging
import os
import pathlib
import shutil
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "../common"))
import installer


def parse_args() -> argparse.Namespace:
    parser = installer.parse_common_args()
    return parser.parse_args()


def get_icon_suffix(config: installer.InstallerConfig) -> str:
    if config.branding == "google_chrome":
        if config.channel == "beta":
            return "_beta"
        elif config.channel == "unstable":
            return "_dev"
        elif config.channel == "canary":
            return "_canary"
    return ""


def main() -> None:
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir).absolute()
    channel = args.channel

    lock_file = output_dir / "snap-lock"
    tmp_file_dir = output_dir / f"snap-{channel}"
    staging_dir = tmp_file_dir / "chrome"

    with installer.StagingContext(tmp_file_dir):
        config = installer.InstallerConfig.from_args(
            args, output_dir, package_format=installer.PackageFormat.SNAP
        )
        config.script_dir = script_dir
        config.staging_dir = staging_dir
        config.tmp_file_dir = tmp_file_dir
        config.shlib_perms = installer.StandardPermissions.REGULAR

        snap_name = config.info_vars["SNAPNAME"]
        install_dir = config.info_vars["INSTALLDIR"]
        config.desktop_exec = snap_name
        config.desktop_icon = (
            f"${{SNAP}}{install_dir}/"
            f"product_logo_256{get_icon_suffix(config)}.png"
        )

        inst = installer.Installer(config)
        inst.prep_staging_common()
        inst.stage_install_common()

        context = config.get_template_context()

        launcher = tmp_file_dir / "chrome.launcher"
        installer.process_template(
            script_dir / "chrome.launcher.in", launcher, context
        )
        launcher.chmod(installer.StandardPermissions.EXECUTABLE)

        desktop_file = tmp_file_dir / f"{snap_name}.desktop"
        installer.process_template(
            output_dir / "installer/common/desktop.template",
            desktop_file,
            context,
        )
        desktop_file.chmod(installer.StandardPermissions.REGULAR)

        installer.process_template(
            script_dir / "snapcraft.yaml.in",
            tmp_file_dir / "snapcraft.yaml",
            context,
        )

        # snapcraft.yaml reads the package version from version.txt.
        shutil.copy(output_dir / "installer/version.txt", tmp_file_dir)

        # Use flock to serialize all executions of snapcraft, as it currently
        # doesn't handle well concurrent builds for the same snap name
        # (https://bugs.launchpad.net/snapcraft/+bug/1869030).
        with lock_file.open("w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            installer.run_command(["snapcraft"], cwd=tmp_file_dir)

        snap_file = f"{snap_name}_{config.version}_{args.arch}.snap"
        package_file = (
            f"{snap_name}-{channel}_{config.version}_{args.arch}.snap"
        )
        shutil.move(tmp_file_dir / snap_file, output_dir / package_file)


if __name__ == "__main__":
    main()
