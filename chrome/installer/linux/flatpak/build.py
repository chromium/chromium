#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import logging
import os
import pathlib
import shutil
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "../common"))
import installer


def parse_args() -> argparse.Namespace:
    parser = installer.parse_common_args()
    parser.add_argument(
        "--flatpak-runtime-version",
        default="24.08",
        help="Freedesktop runtime version",
    )
    return parser.parse_args()


def stage_flatpak_files(
    config: installer.InstallerConfig,
    staging_dir: pathlib.Path,
    output_dir: pathlib.Path,
) -> None:
    files_dir = staging_dir / "files"
    files_dir.mkdir(parents=True, exist_ok=True)

    # Launcher script in files/bin/<flatpak_command>
    bin_dir = files_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    launcher = bin_dir / config.flatpak_command
    installer.process_template(
        output_dir / "installer/flatpak/launcher.template",
        launcher,
        config.get_template_context(),
    )
    launcher.chmod(installer.StandardPermissions.EXECUTABLE)

    # Add binary symlinks for compatibility
    for symlink_name in [
        config.info_vars.get("PACKAGE"),
        config.package_orig,
        config.usr_bin_symlink_name,
        "chrome",
        "chromium",
    ]:
        if symlink_name and symlink_name != config.flatpak_command:
            link = bin_dir / symlink_name
            if not link.exists() and not link.is_symlink():
                os.symlink(config.flatpak_command, link)

    # Desktop file in files/share/applications/<app_id>.desktop
    apps_dir = files_dir / "share/applications"
    apps_dir.mkdir(parents=True, exist_ok=True)
    desktop_file = apps_dir / f"{config.app_id}.desktop"
    installer.process_template(
        output_dir / "installer/common/desktop.template",
        desktop_file,
        config.get_template_context(),
    )
    desktop_file.chmod(installer.StandardPermissions.REGULAR)

    # Metainfo XML in files/share/metainfo/<app_id>.metainfo.xml
    metainfo_dir = files_dir / "share/metainfo"
    metainfo_dir.mkdir(parents=True, exist_ok=True)
    metainfo_file = metainfo_dir / f"{config.app_id}.metainfo.xml"
    installer.process_template(
        output_dir / "installer/flatpak/metainfo.xml.template",
        metainfo_file,
        config.get_template_context(),
    )
    metainfo_file.chmod(installer.StandardPermissions.REGULAR)

    # Icons in files/share/icons/hicolor/<size>x<size>/apps/<app_id>.png
    icon_suffix = ""
    if config.branding == "google_chrome":
        if config.channel == "beta":
            icon_suffix = "_beta"
        elif config.channel == "unstable":
            icon_suffix = "_dev"
        elif config.channel == "canary":
            icon_suffix = "_canary"

    icon_sizes = [16, 24, 32, 48, 64, 128, 256]
    for size in icon_sizes:
        icon_src = (
            output_dir
            / "installer/theme"
            / f"product_logo_{size}{icon_suffix}.png"
        )
        icon_dst_dir = files_dir / "share/icons/hicolor" / f"{size}x{size}/apps"
        icon_dst_dir.mkdir(parents=True, exist_ok=True)
        icon_dst = icon_dst_dir / f"{config.app_id}.png"
        if icon_src.exists():
            shutil.copyfile(icon_src, icon_dst)
            icon_dst.chmod(installer.StandardPermissions.REGULAR)

    # Metadata keyfile in staging_dir/metadata
    metadata_file = staging_dir / "metadata"
    installer.process_template(
        output_dir / "installer/flatpak/metadata.template",
        metadata_file,
        config.get_template_context(),
    )
    metadata_file.chmod(installer.StandardPermissions.REGULAR)


def init_flatpak_repo(repo_dir: pathlib.Path, is_official: bool) -> None:
    for sub in [
        "objects",
        "refs/heads",
        "refs/mirrors",
        "refs/remotes",
        "tmp/cache",
    ]:
        (repo_dir / sub).mkdir(parents=True, exist_ok=True)
    config_file = repo_dir / "config"
    repo_config = (
        "[core]\n"
        "repo_version=1\n"
        "mode=archive\n"
        "min-free-space-percent=0\n"
        "min-free-space-size=1MB\n"
    )
    if not is_official:
        repo_config += "\n[archive]\nzlib-level=0\n"
    config_file.write_text(repo_config)


def main() -> None:
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir).absolute()
    channel = args.channel

    staging_dir = output_dir / f"flatpak-staging-{channel}"
    tmp_file_dir = output_dir / f"flatpak-tmp-{channel}"
    repo_dir = output_dir / f"flatpak-repo-{channel}"

    with installer.StagingContext(staging_dir, tmp_file_dir, repo_dir):
        config = installer.InstallerConfig.from_args(
            args, output_dir, package_format=installer.PackageFormat.FLATPAK
        )
        config.script_dir = script_dir
        # Reusing INSTALLDIR (/opt/...) verbatim under files/ keeps internal
        # relative paths, resource loading, and common artifact staging
        # identical across DEB, RPM, and Flatpak packages. The launcher at
        # /app/bin/<cmd> then execs /app/opt/... directly.
        config.staging_dir = staging_dir / "files"
        config.tmp_file_dir = tmp_file_dir
        config.shlib_perms = installer.StandardPermissions.REGULAR
        config.flatpak_runtime_version = args.flatpak_runtime_version

        config.flatpak_init()

        inst = installer.Installer(config)

        # Stage common files into files/
        inst.prep_staging_common()
        inst.stage_install_common()

        # Stage flatpak-specific files
        stage_flatpak_files(config, staging_dir, output_dir)

        # Finish build directory (exports files, validates metadata)
        installer.run_command(["flatpak", "build-finish", str(staging_dir)])

        # Export build directory to local repository
        init_flatpak_repo(repo_dir, args.official)
        ts = int(args.build_time)
        dt = datetime.datetime.fromtimestamp(ts, datetime.timezone.utc)
        timestamp_str = dt.strftime("%Y-%m-%dT%H:%M:%SZ")

        export_cmd = [
            "flatpak",
            "build-export",
            f"--arch={args.arch}",
            f"--timestamp={timestamp_str}",
            str(repo_dir),
            str(staging_dir),
            config.channel,
        ]
        installer.run_command(export_cmd)

        # Create single-file flatpak bundle
        bundle_file = output_dir / config.flatpak_package_filename
        bundle_cmd = [
            "flatpak",
            "build-bundle",
            f"--arch={args.arch}",
            str(repo_dir),
            str(bundle_file),
            config.app_id,
            config.channel,
        ]
        installer.run_command(bundle_cmd)
        logging.info(f"Built {bundle_file}")


if __name__ == "__main__":
    main()
