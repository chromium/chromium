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


def get_config_dir_name(config: installer.InstallerConfig) -> str:
    # The basename of the default user data directory.
    # LINT.IfChange(ConfigDirName)
    if config.branding == "chromium":
        return "chromium"
    assert config.branding == "google_chrome"
    suffix = {
        "stable": "",
        "beta": "-beta",
        "unstable": "-unstable",
        "canary": "-canary",
    }[config.channel]
    return f"google-chrome{suffix}"
    # LINT.ThenChange(
    #     //chrome/common/chrome_paths_linux.cc:DataDirBasename,
    #     //chrome/common/channel_info_posix.cc:ChannelSuffixForDataDir
    # )


def get_config_plug_name(config: installer.InstallerConfig) -> str:
    # The name of the personal-files plug used for importing an existing
    # profile. These match the plug names in Canonical's Chromium and Google
    # Chrome snaps, since store auto-connection is granted per plug name.
    if config.branding == "chromium":
        return "chromium-config"
    assert config.branding == "google_chrome"
    return "chrome-config"


def stage_snap_files(
    config: installer.InstallerConfig,
    script_dir: pathlib.Path,
    output_dir: pathlib.Path,
    project_dir: pathlib.Path,
) -> None:
    snap_name = config.info_vars["SNAPNAME"]
    install_dir = config.info_vars["INSTALLDIR"]

    # The app is named after the snap, so the snap's desktop file must be too.
    # Chrome derives its WM class from the desktop file name (see
    # chrome::GetDesktopName()).
    config.desktop_exec = snap_name
    config.desktop_icon = (
        f"${{SNAP}}{install_dir}/product_logo_256{get_icon_suffix(config)}.png"
    )
    config.startup_wm_class = snap_name

    context = config.get_template_context()
    context.update(
        {
            "snap_config_dir": get_config_dir_name(config),
            "snap_config_plug": get_config_plug_name(config),
            "snap_install_dir": install_dir.lstrip("/"),
        }
    )

    # The launcher and desktop file are installed into bin/ by the "launcher"
    # part.
    launcher_dir = project_dir / "launcher"
    launcher_dir.mkdir(parents=True, exist_ok=True)

    launcher = launcher_dir / "chrome.launcher"
    installer.process_template(
        script_dir / "chrome.launcher.in", launcher, context
    )
    launcher.chmod(installer.StandardPermissions.EXECUTABLE)

    desktop_file = launcher_dir / f"{snap_name}.desktop"
    installer.process_template(
        output_dir / "installer/common/desktop.template",
        desktop_file,
        context,
    )
    desktop_file.chmod(installer.StandardPermissions.REGULAR)

    # The man page is looked up in $SNAP/man1 (see MANPATH in the launcher).
    # The browser runs `man` with the basename of the executable.
    man_dir = config.staging_dir / "man1"
    man_dir.mkdir(parents=True, exist_ok=True)
    man_page = man_dir / f"{config.info_vars['PROGNAME']}.1"
    installer.process_template(
        output_dir / "installer/common/manpage.1.in", man_page, context
    )
    installer.run_command(["gzip", "-9nf", str(man_page)])
    (man_dir / f"{man_page.name}.gz").chmod(
        installer.StandardPermissions.REGULAR
    )

    installer.process_template(
        script_dir / "snapcraft.yaml.in",
        project_dir / "snapcraft.yaml",
        context,
    )


def main() -> None:
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir).absolute()
    channel = args.channel

    lock_file = output_dir / "snap-lock"
    # The snapcraft project directory.
    project_dir = output_dir / f"snap-{channel}"
    staging_dir = project_dir / "chrome"

    with installer.StagingContext(project_dir):
        config = installer.InstallerConfig.from_args(
            args, output_dir, package_format=installer.PackageFormat.SNAP
        )
        config.script_dir = script_dir
        config.staging_dir = staging_dir
        config.tmp_file_dir = project_dir
        config.shlib_perms = installer.StandardPermissions.REGULAR

        inst = installer.Installer(config)
        inst.prep_staging_common()
        inst.stage_install_common()

        stage_snap_files(config, script_dir, output_dir, project_dir)

        # Use flock to serialize all executions of snapcraft, as it currently
        # doesn't handle well concurrent builds for the same snap name
        # (https://bugs.launchpad.net/snapcraft/+bug/1869030).
        with lock_file.open("w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            installer.run_command(["snapcraft", "pack"], cwd=project_dir)

        snap_name = config.info_vars["SNAPNAME"]
        snap_file = f"{snap_name}_{config.version}_{args.arch}.snap"
        package_file = (
            f"{snap_name}-{channel}_{config.version}_{args.arch}.snap"
        )
        shutil.move(project_dir / snap_file, output_dir / package_file)


if __name__ == "__main__":
    main()
