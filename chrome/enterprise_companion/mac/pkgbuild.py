#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
import platform
import shutil
import subprocess
import sys
import tempfile
import os

# This script runs pkgbuild to create a MacOS PKG installer from a directory.


def main():
    if platform.system() != 'Darwin':
        print("ERROR: pkgbuild is only available on MacOS.", file=sys.stderr)
        return 1

    parser = OptionParser()
    parser.add_option(
        "--root-path",
        dest="root_path",
        help=
        "Package the entire contents of the directory tree at root-path, typically an application bundle."
    )
    parser.add_option("--identifier",
                      dest="pkg_identifier",
                      help="Specify a unique identifier for this package.")
    parser.add_option(
        "--install-location",
        dest="install_path",
        help=
        "Specify the default install location for the contents of the package."
    )
    parser.add_option(
        "--postinstall-script",
        dest="postinstall_script_path",
        help=
        "Specify a postinstall script to be run as a top-level script in the package."
    )
    parser.add_option("--package-output-path",
                      dest="package_output_path",
                      help="The path to which the package will be written.")
    parser.add_option(
        "--sign-identity-name",
        dest="sign_identity_name",
        help="Adds a digital signature to the resulting package.")
    (options, _) = parser.parse_args()

    if not options.root_path or not options.pkg_identifier or not options.install_path or not options.package_output_path:
        parser.error("Missing required flag")

    with tempfile.TemporaryDirectory() as scripts_dir:
        argv = [
            "pkgbuild", "--root", options.root_path, "--identifier",
            options.pkg_identifier, "--install-location", options.install_path
        ]

        if options.postinstall_script_path != None:
            shutil.copy2(options.postinstall_script_path,
                         os.path.join(scripts_dir, "postinstall"))
            argv += ["--scripts", scripts_dir]

        if options.sign_identity_name != None:
            argv += ["--sign", options.sign_identity_name]

        argv += [options.package_output_path]
        return subprocess.call(argv)


if __name__ == '__main__':
    sys.exit(main())
