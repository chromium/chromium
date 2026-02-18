#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pathlib
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "common"))
import installer


def get_pgp_key_version(common_dir: pathlib.Path) -> str:
    key_include = common_dir / "key.include"
    with key_include.open("r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("PGP_KEY_VERSION="):
                return line.split("=")[1].strip()
    raise ValueError(f"PGP_KEY_VERSION not found in {key_include}")


def main() -> None:
    parser = installer.parse_common_args()
    parser.add_argument("-s", "--sysroot", required=True, help="sysroot")
    args = parser.parse_args()

    output_dir = pathlib.Path(args.output_dir)
    script_dir = pathlib.Path(__file__).parent.absolute()
    common_dir = script_dir.parent / "common"

    pgp_key_version = get_pgp_key_version(common_dir)

    config = installer.InstallerConfig.from_args(args, output_dir)

    # Override things for the repo package
    config.versionfull = pgp_key_version
    config.priority = "10"

    if args.branding == "google_chrome":
        config.info_vars["INSTALLDIR"] = "/opt/google/chrome-repo"
        config.info_vars["PACKAGE"] = "google-chrome-repo"
    else:
        config.info_vars["INSTALLDIR"] = "/opt/chromium.org/chromium-repo"
        config.info_vars["PACKAGE"] = "chromium-browser-repo"

    config.channel = "repo"

    staging_dir = output_dir / "deb-repo-staging"
    tmp_file_dir = output_dir / "deb-repo-tmp"

    with installer.StagingContext(staging_dir, tmp_file_dir):
        staging_dir.chmod(0o755)

        debian_dir = staging_dir / "DEBIAN"
        debian_dir.mkdir(parents=True)
        debian_dir.chmod(0o755)

        context = config.get_template_context()
        context["priority"] = config.priority

        # Use the templates we just created.
        # They should be in the same directory as this script in the source
        # tree, but also copied to the output directory. installer.py handles
        # absolute paths fine.
        installer.process_template(
            script_dir / "control_repo.template",
            debian_dir / "control",
            context,
        )
        with (debian_dir / "control").open("a") as f:
            f.write("\n")
        installer.process_template(script_dir / "postinst_repo",
                                   debian_dir / "postinst", context)
        installer.process_template(script_dir / "postrm_repo",
                                   debian_dir / "postrm", context)

        (debian_dir / "postinst").chmod(0o755)
        (debian_dir / "postrm").chmod(0o755)

        pkg_name = config.info_vars["PACKAGE"]
        deb_file = output_dir / f"{pkg_name}_{pgp_key_version}_all.deb"

        # Use fakeroot to ensure correct permissions in the debian package.
        installer.run_command(
            ["fakeroot", "dpkg-deb", "-b",
             str(staging_dir),
             str(deb_file)])


if __name__ == "__main__":
    main()
