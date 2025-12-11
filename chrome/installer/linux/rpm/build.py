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


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-a", "--arch", required=True, help="rpm package architecture")
    parser.add_argument(
        "-b", "--build-time", required=True, help="build timestamp")
    parser.add_argument(
        "-c", "--channel", required=True, help="package channel")
    parser.add_argument("-d", "--branding", required=True, help="branding")
    parser.add_argument(
        "-f", "--official", action="store_true", help="official build")
    parser.add_argument(
        "-o", "--output-dir", required=True, help="output directory")
    parser.add_argument("-t", "--target-os", required=True, help="target os")
    return parser.parse_args()


def gen_spec(context, spec_file):
    if spec_file.exists():
        spec_file.unlink()

    context[
        "PACKAGE_FILENAME"] = f"{context['PACKAGE_ORIG']}-{context['CHANNEL']}"

    script_dir = pathlib.Path(__file__).parent.absolute()
    installer.process_template(script_dir / "chrome.spec.template", spec_file,
                               context)


def verify_package(context, rpm_file):
    depends = context["DEPENDS"]
    additional_deps = [
        "/bin/sh",
        "rpmlib(CompressedFileNames) <= 3.0.4-1",
        "rpmlib(FileDigests) <= 4.6.0-1",
        "rpmlib(PayloadFilesHavePrefix) <= 4.0-1",
        "/usr/sbin/update-alternatives",
    ]
    if context["IS_OFFICIAL_BUILD"] == "1":
        additional_deps.append("rpmlib(PayloadIsXz) <= 5.2-1")

    all_deps = depends.split(",") + additional_deps
    expected_depends = [d.strip() for d in all_deps if d.strip()]

    output = subprocess.check_output(["rpm", "-qpR", str(rpm_file)], text=True)
    actual_depends = output.splitlines()

    installer.verify_package_deps(expected_depends, actual_depends)


def main():
    os.umask(0o022)
    args = parse_args()

    log_level = logging.INFO if os.environ.get("VERBOSE") else logging.ERROR
    logging.basicConfig(level=log_level, format="%(message)s")

    script_dir = pathlib.Path(__file__).parent.absolute()
    output_dir = pathlib.Path(args.output_dir).absolute()
    channel = args.channel

    staging_dir = output_dir / f"rpm-staging-{channel}"
    tmp_file_dir = output_dir / f"rpm-tmp-{channel}"
    rpmbuild_dir = output_dir / f"rpm-build-{channel}"

    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    if tmp_file_dir.exists():
        shutil.rmtree(tmp_file_dir)
    if rpmbuild_dir.exists():
        shutil.rmtree(rpmbuild_dir)

    staging_dir.mkdir(parents=True, exist_ok=True)
    tmp_file_dir.mkdir(parents=True, exist_ok=True)
    rpmbuild_dir.mkdir(parents=True, exist_ok=True)

    spec_file = tmp_file_dir / "chrome.spec"

    inst = installer.Installer(
        output_dir,
        staging_dir,
        channel,
        args.branding,
        args.arch,
        args.target_os,
        args.official,
    )

    inst.set_context({
        "OUTPUTDIR": output_dir,
        "STAGEDIR": staging_dir,
        "TMPFILEDIR": tmp_file_dir,
        "RPMBUILD_DIR": rpmbuild_dir,
        "BRANDING": args.branding,
        "ARCHITECTURE": args.arch,
        "BUILD_TIMESTAMP": args.build_time,
        "IS_OFFICIAL_BUILD": "1" if args.official else "0",
        "SCRIPTDIR": script_dir,
        "SPEC": spec_file,
    })

    inst.initialize()
    channel = inst.channel

    os.environ["ARCHITECTURE"] = args.arch

    # REPOCONFIG logic
    # ${PACKAGE#google-} removes "google-" prefix.
    package = inst.context["PACKAGE_ORIG"]
    if package.startswith("google-"):
        repo_package = package[7:]
    else:
        repo_package = package
    inst.context[
        "REPOCONFIG"] = f"https://dl.google.com/linux/{repo_package}/rpm/stable"
    inst.context["REPOCONFIGREGEX"] = ""

    inst.context["SHLIB_PERMS"] = 0o755

    inst.prep_staging_common()
    (staging_dir / "etc/cron.daily").mkdir(parents=True, exist_ok=True)
    (staging_dir / "etc/cron.daily").chmod(0o755)

    inst.stage_install_common()

    logging.info(f"Staging RPM install files in '{staging_dir}'...")
    cron_file = staging_dir / "etc/cron.daily" / inst.context["PACKAGE"]
    installer.process_template(
        output_dir / "installer/common/rpmrepo.cron",
        cron_file,
        inst.context,
    )
    cron_file.chmod(0o755)

    # do_package logic
    logging.info(f"Packaging {args.arch}...")

    inst.context["PROVIDES"] = inst.context["PACKAGE_ORIG"]

    rpm_common_deps_file = output_dir / "rpm_common.deps"
    with rpm_common_deps_file.open("r") as f:
        depends = f.read().replace("\n", ",")
    inst.context["DEPENDS"] = depends

    if inst.context["PACKAGE"] != inst.context["USR_BIN_SYMLINK_NAME"]:
        symlink = inst.context['USR_BIN_SYMLINK_NAME']
        inst.context[
            "OPTIONAL_MAN_PAGE"] = f"/usr/share/man/man1/{symlink}.1.gz"
    else:
        inst.context["OPTIONAL_MAN_PAGE"] = ""

    gen_spec(inst.context, spec_file)

    os.environ["SOURCE_DATE_EPOCH"] = args.build_time

    (rpmbuild_dir / "BUILD").mkdir(parents=True, exist_ok=True)
    (rpmbuild_dir / "RPMS").mkdir(parents=True, exist_ok=True)

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
        f"--define=_topdir {rpmbuild_dir}",
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

    pkg_name = (f"{inst.context['PACKAGE_FILENAME']}-{inst.context['VERSION']}-"
                f"{inst.context['PACKAGE_RELEASE']}")
    rpm_file = f"{pkg_name}.{args.arch}.rpm"
    src_rpm = rpmbuild_dir / f"RPMS/{args.arch}/{rpm_file}"
    dst_rpm = output_dir / rpm_file

    shutil.move(src_rpm, dst_rpm)
    dst_rpm.chmod(0o644)

    verify_package(inst.context, dst_rpm)

    # cleanup
    shutil.rmtree(staging_dir)
    shutil.rmtree(tmp_file_dir)
    shutil.rmtree(rpmbuild_dir)


if __name__ == "__main__":
    main()
