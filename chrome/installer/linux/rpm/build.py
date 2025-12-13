#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import time


sys.path.append(os.path.join(os.path.dirname(__file__), "../common"))
import installer


def parse_args() -> argparse.Namespace:
    parser = installer.parse_common_args()
    return parser.parse_args()


def gen_spec(config: installer.InstallerConfig,
             spec_file: pathlib.Path) -> None:
    if spec_file.exists():
        spec_file.unlink()

    config.rpm_package_filename = f"{config.package_orig}-{config.channel}"

    script_dir = pathlib.Path(__file__).parent.absolute()
    installer.process_template(
        script_dir / "chrome.spec.template",
        spec_file,
        config.get_template_context(),
    )


def verify_package(config: installer.InstallerConfig,
                   rpm_file: pathlib.Path) -> None:
    depends = config.rpm_depends
    additional_deps = [
        "/bin/sh",
        "rpmlib(CompressedFileNames) <= 3.0.4-1",
        "rpmlib(FileDigests) <= 4.6.0-1",
        "rpmlib(PayloadFilesHavePrefix) <= 4.0-1",
        "/usr/sbin/update-alternatives",
    ]
    if config.is_official_build:
        additional_deps.append("rpmlib(PayloadIsXz) <= 5.2-1")

    all_deps = depends.split(",") + additional_deps
    expected_depends = [d.strip() for d in all_deps if d.strip()]

    output = subprocess.check_output(["rpm", "-qpR", str(rpm_file)], text=True)
    actual_depends = output.splitlines()

    installer.verify_package_deps(expected_depends, actual_depends)


def main() -> None:
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir).absolute()
    channel = args.channel

    staging_dir = output_dir / f"rpm-staging-{channel}"
    tmp_file_dir = output_dir / f"rpm-tmp-{channel}"
    rpm_build_dir = output_dir / f"rpm-build-{channel}"

    with installer.StagingContext(staging_dir, tmp_file_dir, rpm_build_dir):
        spec_file = tmp_file_dir / "chrome.spec"

        config = installer.InstallerConfig.from_args(args, output_dir)
        config.script_dir = script_dir
        config.staging_dir = staging_dir
        config.tmp_file_dir = tmp_file_dir
        config.rpm_build_dir = rpm_build_dir
        config.rpm_spec_file = spec_file

        inst = installer.Installer(config)

        inst.prep_staging_common()
        (staging_dir / "etc/cron.daily").mkdir(parents=True, exist_ok=True)
        (staging_dir / "etc/cron.daily").chmod(
            installer.StandardPermissions.EXECUTABLE)

        inst.stage_install_common()

        logging.info(f"Staging RPM install files in '{staging_dir}'...")
        cron_file = staging_dir / "etc/cron.daily" / config.info_vars["PACKAGE"]
        installer.process_template(
            output_dir / "installer/common/rpmrepo.cron",
            cron_file,
            config.get_template_context(),
        )
        cron_file.chmod(installer.StandardPermissions.EXECUTABLE)

        # do_package logic
        logging.info(f"Packaging {args.arch}...")

        config.rpm_provides = config.package_orig

        rpm_common_deps_file = output_dir / "rpm_common.deps"
        with rpm_common_deps_file.open("r") as f:
            depends = f.read().replace("\n", ",")
        config.rpm_depends = depends

        if config.info_vars["PACKAGE"] != config.usr_bin_symlink_name:
            symlink = config.usr_bin_symlink_name
            config.rpm_optional_man_page = f"/usr/share/man/man1/{symlink}.1.gz"

        gen_spec(config, spec_file)

        os.environ["SOURCE_DATE_EPOCH"] = args.build_time

        (rpm_build_dir / "BUILD").mkdir(parents=True, exist_ok=True)
        (rpm_build_dir / "RPMS").mkdir(parents=True, exist_ok=True)

        if args.official:
            compression_opt = "_binary_payload w9.xzdio"
        else:
            compression_opt = "_binary_payload w0.gzdio"

        cmd = [
            "fakeroot",
            "rpmbuild",
            "-bb",
            f"--target={args.arch}",
            "--rmspec",
            f"--define=_topdir {rpm_build_dir}",
            f"--define={compression_opt}",
            "--define=__os_install_post %{nil}",
            "--define=_build_id_links none",
            "--define=build_mtime_policy clamp_to_source_date_epoch",
            "--define=clamp_mtime_to_source_date_epoch 1",
            "--define=use_source_date_epoch_as_buildtime 1",
            "--define=_buildhost reproducible",
            str(spec_file),
        ]

        installer.run_command(cmd)

        pkg_name = (f"{config.rpm_package_filename}-{config.version}-"
                    f"{config.package_release}")
        rpm_file = f"{pkg_name}.{args.arch}.rpm"
        src_rpm = rpm_build_dir / f"RPMS/{args.arch}/{rpm_file}"
        dst_rpm = output_dir / rpm_file

        shutil.move(src_rpm, dst_rpm)
        dst_rpm.chmod(installer.StandardPermissions.REGULAR)

        verify_package(config, dst_rpm)


if __name__ == "__main__":
    main()
