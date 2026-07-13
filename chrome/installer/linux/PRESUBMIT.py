# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting chrome/installer/linux/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import shutil
import sys
import tempfile


def _CheckRepoPackageVersionBump(input_api, output_api):
    if not sys.platform.startswith("linux"):
        return []

    missing_deps = []
    if not shutil.which("fakeroot"):
        missing_deps.append("fakeroot")
    if not shutil.which("dpkg-deb"):
        missing_deps.append("dpkg-deb")
    if missing_deps:
        return [
            output_api.PresubmitError(
                f"Missing required executables for deterministic package build check: "
                f"{', '.join(missing_deps)}. Please install them to proceed.")
        ]

    if not os.path.exists(input_api.PresubmitLocalPath()):
        return [
            output_api.PresubmitError(
                f"PresubmitLocalPath {input_api.PresubmitLocalPath()} does not exist."
            )
        ]

    repo_include_path = "chrome/installer/linux/debian/repo_package.include"
    repo_file = next(
        (f for f in input_api.AffectedFiles(include_deletes=False)
         if f.LocalPath().replace("\\", "/") == repo_include_path),
        None,
    )

    expected_hash = None
    if repo_file:
        for line in repo_file.NewContents():
            line = line.strip()
            if line.startswith("REPO_PACKAGE_HASH="):
                expected_hash = line.split("=")[1].strip()
                break
    else:
        abs_repo_include = os.path.join(input_api.PresubmitLocalPath(),
                                        "debian", "repo_package.include")
        if os.path.exists(abs_repo_include):
            with open(abs_repo_include, "r") as f:
                for line in f:
                    line = line.strip()
                    if line.startswith("REPO_PACKAGE_HASH="):
                        expected_hash = line.split("=")[1].strip()
                        break

    if expected_hash is None:
        return [
            output_api.PresubmitError(
                f"REPO_PACKAGE_HASH not found in {repo_include_path}.")
        ]

    sys.path.insert(0, os.path.join(input_api.PresubmitLocalPath(), "common"))
    try:
        import installer
    finally:
        sys.path.pop(0)

    with tempfile.TemporaryDirectory() as tmpdir:
        try:
            actual_hash = installer.compute_repo_package_hash_for_presubmit(
                input_api.PresubmitLocalPath(), tmpdir)
        except RuntimeError as e:
            return [
                output_api.PresubmitError(
                    f"Failed to build repo package for presubmit check:\n{e}")
            ]

    if actual_hash == expected_hash:
        return []

    return [
        output_api.PresubmitError(
            f"The generated google-chrome-repo / chromium-browser-repo "
            f"package hash ({actual_hash}) does not match REPO_PACKAGE_HASH "
            f"in {repo_include_path} ({expected_hash}).\n"
            "Please run chrome/installer/linux/common/update_key_include.py "
            "--repo-only to automatically update debian/repo_package.include.")
    ]


def _CommonChecks(input_api, output_api):
    results = []
    results.extend(_CheckRepoPackageVersionBump(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
