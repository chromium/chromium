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


sys.path.append(os.path.join(os.path.dirname(__file__), "..", "common"))
import installer


def parse_args() -> argparse.Namespace:
    parser = installer.parse_common_args()
    parser.add_argument("-s", "--sysroot", required=True, help="sysroot")
    parser.add_argument(
        "-T",
        "--xz-nthreads",
        required=False,
        help="Number of threads to run xz",
        type=int,
        default=0)
    return parser.parse_args()


def gen_control(
    config: installer.InstallerConfig,
    staging_dir: pathlib.Path,
    deb_control: pathlib.Path,
    deb_changelog: pathlib.Path,
    deb_files: pathlib.Path,
    pkg_name: str,
) -> None:
    cmd = [
        "dpkg-gencontrol",
        f"-v{config.versionfull}",
        f"-c{deb_control}",
        f"-l{deb_changelog}",
        f"-f{deb_files}",
        f"-p{pkg_name}",
        f"-P{staging_dir}",
        "-O",
    ]

    output_control = staging_dir / "DEBIAN/control"
    with output_control.open("w") as f:
        if os.environ.get("VERBOSE"):
            subprocess.check_call(cmd, stdout=f)
        else:
            subprocess.check_call(cmd, stdout=f, stderr=subprocess.DEVNULL)

    deb_control.unlink()


def verify_package(config: installer.InstallerConfig,
                   deb_file: pathlib.Path) -> None:
    depends = config.deb_depends
    expected_depends = [d.strip() for d in depends.split(", ") if d.strip()]

    output = subprocess.check_output(["dpkg", "-I", str(deb_file)], text=True)
    actual_depends = []
    for line in output.splitlines():
        if line.startswith(" Depends: "):
            deps = line[len(" Depends: "):].split(", ")
            actual_depends = [d.strip() for d in deps if d.strip()]
            break

    installer.verify_package_deps(expected_depends, actual_depends)


def main() -> None:
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir)

    staging_dir = output_dir / f"deb-staging-{args.channel}"
    tmp_file_dir = output_dir / f"deb-tmp-{args.channel}"

    with installer.StagingContext(staging_dir, tmp_file_dir):
        deb_changelog = tmp_file_dir / "changelog"
        deb_files = tmp_file_dir / "files"
        deb_control = tmp_file_dir / "control"

        config = installer.InstallerConfig.from_args(args, output_dir)
        config.script_dir = script_dir
        config.staging_dir = staging_dir
        config.tmp_file_dir = tmp_file_dir
        config.shlib_perms = installer.StandardPermissions.REGULAR

        # Calculate deps
        deb_common_deps_file = output_dir / "deb_common.deps"
        with deb_common_deps_file.open("r") as f:
            common_deps = f.read().strip().replace("\n", ", ")
        config.common_deps = common_deps
        config.common_predeps = "dpkg (>= 1.14.0)"

        inst = installer.Installer(config)

        # Export variables for dpkg tools
        os.environ["ARCHITECTURE"] = args.arch
        os.environ["DEBEMAIL"] = config.info_vars["MAINTMAIL"]
        os.environ["DEBFULLNAME"] = config.info_vars["MAINTNAME"]

        # Prep staging debian
        inst.prep_staging_common()
        (staging_dir / "DEBIAN").mkdir(parents=True, exist_ok=True)
        (staging_dir / "DEBIAN").chmod(installer.StandardPermissions.EXECUTABLE)
        (staging_dir / "etc/cron.daily").mkdir(parents=True, exist_ok=True)
        (staging_dir / "etc/cron.daily").chmod(
            installer.StandardPermissions.EXECUTABLE)
        (staging_dir / f"usr/share/doc/{config.usr_bin_symlink_name}").mkdir(
            parents=True, exist_ok=True)
        (staging_dir / f"usr/share/doc/{config.usr_bin_symlink_name}").chmod(
            installer.StandardPermissions.EXECUTABLE)

        inst.stage_install_common()

        logging.info(f"Staging Debian install files in '{staging_dir}'...")
        install_dir = staging_dir / config.info_vars["INSTALLDIR"].lstrip("/")
        cron_dir = install_dir / "cron"
        cron_dir.mkdir(parents=True, exist_ok=True)
        cron_dir.chmod(installer.StandardPermissions.EXECUTABLE)

        cron_file = cron_dir / config.info_vars["PACKAGE"]
        installer.process_template(
            output_dir / "installer/common/repo.cron",
            cron_file,
            config.get_template_context(),
        )
        cron_file.chmod(installer.StandardPermissions.EXECUTABLE)

        cron_daily_link = (
            staging_dir / "etc/cron.daily" / config.info_vars["PACKAGE"])
        if cron_daily_link.is_symlink() or cron_daily_link.exists():
            cron_daily_link.unlink()
        os.symlink(
            os.path.join(
                config.info_vars["INSTALLDIR"],
                "cron",
                config.info_vars["PACKAGE"],
            ),
            cron_daily_link,
        )

        for script in ["postinst", "prerm", "postrm"]:
            dest = staging_dir / "DEBIAN" / script
            installer.process_template(
                output_dir / f"installer/debian/{script}",
                dest,
                config.get_template_context(),
            )
            dest.chmod(installer.StandardPermissions.EXECUTABLE)

        # Restore PACKAGE for control template
        config.info_vars["PACKAGE"] = config.package_orig

        logging.info(f"Packaging {args.arch}...")
        config.deb_pre_depends = config.common_predeps
        config.deb_depends = config.common_deps
        config.deb_provides = "www-browser"

        installer.gen_changelog(config, deb_changelog)
        installer.process_template(
            script_dir / "control.template",
            deb_control,
            config.get_template_context(),
        )

        os.environ["DEB_HOST_ARCH"] = args.arch
        pkg_name = f"{config.package_orig}-{config.channel}"

        if deb_control.exists():
            gen_control(
                config,
                staging_dir,
                deb_control,
                deb_changelog,
                deb_files,
                pkg_name,
            )

        os.environ["SOURCE_DATE_EPOCH"] = args.build_time
        staging_dir.chmod(0o750)
        installer.run_command([
            "fakeroot",
            "dpkg-deb",
            "-Znone",
            "-b",
            str(staging_dir),
            str(tmp_file_dir),
        ])

        package_file = f"{pkg_name}_{config.versionfull}_{args.arch}.deb"
        package_path = tmp_file_dir / package_file

        if args.official:
            pkg_basename = package_path.name
            installer.run_command(["ar", "-x", pkg_basename], cwd=tmp_file_dir)
            installer.run_command([
                "xz",
                "-z9",
                f"-T{args.xz_nthreads}",
                "--lzma2=dict=256MiB",
                str(tmp_file_dir / "data.tar"),
            ])
            installer.run_command(
                ["xz", "-z0", str(tmp_file_dir / "control.tar")])
            installer.run_command(
                ["ar", "-d", pkg_basename, "control.tar", "data.tar"],
                cwd=tmp_file_dir,
            )
            installer.run_command(
                ["ar", "-r", pkg_basename, "control.tar.xz", "data.tar.xz"],
                cwd=tmp_file_dir,
            )

        final_package_path = output_dir / package_file
        shutil.move(package_path, final_package_path)

        verify_package(config, final_package_path)


if __name__ == "__main__":
    main()
