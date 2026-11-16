#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import gzip
import io
import lzma
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT_DIR = pathlib.Path(__file__).parent.absolute()
LINUX_INSTALLER_DIR = SCRIPT_DIR.parent
sys.path.insert(0, str(SCRIPT_DIR))
sys.path.insert(0, str(LINUX_INSTALLER_DIR / "common"))

import build  # noqa: E402
import installer  # noqa: E402

RPM_V4_LEAD = b"\xed\xab\xee\xdb\x03\x00" + b"\x00" * 90
RPM_V6_LEAD = b"\xed\xab\xee\xdb\x04\x00" + b"\x00" * 90


class BuildTest(unittest.TestCase):
    def _make_config(
        self, tmp_path: pathlib.Path, *, official: bool = False
    ) -> installer.InstallerConfig:
        config = installer.InstallerConfig(
            output_dir=tmp_path,
            staging_dir=tmp_path / "staging",
            channel="stable",
            branding="chromium",
            arch="x86_64",
            target_os="linux",
            is_official_build=official,
            shlib_perms=installer.StandardPermissions.EXECUTABLE,
            package_format=installer.PackageFormat.RPM,
        )
        config.rpm_depends = (
            "rpmlib(FileDigests) <= 4.6.0-1,"
            "rpmlib(RichDependencies) <= 4.12.0-1"
        )
        return config

    def test_verify_package_accepts_rpm_v4(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = pathlib.Path(tmpdir)
            rpm_file = tmp_path / "test.rpm"
            rpm_file.write_bytes(RPM_V4_LEAD)
            config = self._make_config(tmp_path, official=True)

            v4_deps = "\n".join(
                [
                    "/bin/sh",
                    "/usr/sbin/update-alternatives",
                    "rpmlib(CompressedFileNames) <= 3.0.4-1",
                    "rpmlib(FileDigests) <= 4.6.0-1",
                    "rpmlib(PayloadFilesHavePrefix) <= 4.0-1",
                    "rpmlib(PayloadIsXz) <= 5.2-1",
                    "rpmlib(RichDependencies) <= 4.12.0-1",
                ]
            )
            with mock.patch.object(
                subprocess, "check_output", return_value=v4_deps + "\n"
            ):
                build.verify_package(config, rpm_file)

    def test_verify_package_rejects_rpm_v6_lead(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = pathlib.Path(tmpdir)
            rpm_file = tmp_path / "test_v6.rpm"
            rpm_file.write_bytes(RPM_V6_LEAD)
            config = self._make_config(tmp_path)

            v4_deps = "\n".join(
                [
                    "/bin/sh",
                    "/usr/sbin/update-alternatives",
                    "rpmlib(CompressedFileNames) <= 3.0.4-1",
                    "rpmlib(FileDigests) <= 4.6.0-1",
                    "rpmlib(PayloadFilesHavePrefix) <= 4.0-1",
                    "rpmlib(RichDependencies) <= 4.12.0-1",
                ]
            )
            with (
                mock.patch.object(
                    subprocess, "check_output", return_value=v4_deps + "\n"
                ),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                with self.assertRaises(SystemExit) as ctx:
                    build.verify_package(config, rpm_file)
                self.assertNotEqual(ctx.exception.code, 0)

    def test_verify_package_rejects_rpm_v6_deps(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = pathlib.Path(tmpdir)
            rpm_file = tmp_path / "test.rpm"
            rpm_file.write_bytes(RPM_V4_LEAD)
            config = self._make_config(tmp_path)

            v6_deps = "\n".join(
                [
                    "/bin/sh",
                    "/usr/sbin/update-alternatives",
                    "rpmlib(FileDigests) <= 4.6.0-1",
                    "rpmlib(LargeFiles) <= 4.12.0-1",
                    "rpmlib(RichDependencies) <= 4.12.0-1",
                ]
            )
            with (
                mock.patch.object(
                    subprocess, "check_output", return_value=v6_deps + "\n"
                ),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                with self.assertRaises(SystemExit) as ctx:
                    build.verify_package(config, rpm_file)
                self.assertNotEqual(ctx.exception.code, 0)

    def _stage_minimal_build_output(self, out_dir: pathlib.Path) -> None:
        common_out = out_dir / "installer/common"
        theme_out = out_dir / "installer/theme"
        locales_out = out_dir / "locales"
        common_out.mkdir(parents=True, exist_ok=True)
        theme_out.mkdir(parents=True, exist_ok=True)
        locales_out.mkdir(parents=True, exist_ok=True)

        for src in (LINUX_INSTALLER_DIR / "common").iterdir():
            if src.is_file() and not src.name.endswith(".py"):
                shutil.copy(src, common_out / src.name)
        shutil.copy(
            LINUX_INSTALLER_DIR.parent.parent / "app/resources/manpage.1.in",
            common_out / "manpage.1.in",
        )

        (theme_out / "BRANDING").write_text(
            "COMPANY_FULLNAME=The Chromium Authors\n"
            "COMPANY_SHORTNAME=Chromium\n"
            "PRODUCT_FULLNAME=Chromium\n"
            "PRODUCT_SHORTNAME=Chromium\n"
            "PRODUCT_INSTALLER_FULLNAME=Chromium Installer\n"
            "PRODUCT_INSTALLER_SHORTNAME=Chromium Installer\n"
            "COPYRIGHT=Copyright The Chromium Authors\n"
            "MAC_BUNDLE_ID=org.chromium.Chromium\n"
            "MAC_CREATOR_CODE=Cr24\n"
            "MAC_TEAM_ID=\n"
        )
        for size in (16, 24, 32, 48, 64, 128, 256):
            (theme_out / f"product_logo_{size}.png").write_bytes(b"png")

        (out_dir / "installer/version.txt").write_text(
            "MAJOR=160\nMINOR=0\nBUILD=8155\nPATCH=0\n"
        )
        (out_dir / "rpm_common.deps").write_text(
            "rpmlib(FileDigests) <= 4.6.0-1\n"
            "rpmlib(RichDependencies) <= 4.12.0-1\n"
        )

        for binary in (
            "chrome.stripped",
            "chrome_crashpad_handler.stripped",
            "chrome_management_service.stripped",
            "chrome_sandbox.stripped",
        ):
            path = out_dir / binary
            path.write_text("#!/bin/sh\nexit 0\n")
            path.chmod(0o755)

        for resource in (
            "resources.pak",
            "icudtl.dat",
            "chrome_100_percent.pak",
            "chrome_200_percent.pak",
            "v8_context_snapshot.bin",
            "locales/en-US.pak",
        ):
            (out_dir / resource).write_bytes(b"pak")

    @unittest.skipUnless(
        sys.platform.startswith("linux")
        and all(
            shutil.which(tool)
            for tool in ("rpmbuild", "rpm", "file", "readelf", "gzip")
        ),
        "Linux RPM packaging tools required",
    )
    def test_end_to_end_builds_rpm_v4_package(self):
        for official in (False, True):
            with (
                self.subTest(official=official),
                tempfile.TemporaryDirectory() as tmpdir,
            ):
                out_dir = pathlib.Path(tmpdir)
                self._stage_minimal_build_output(out_dir)

                argv = [
                    "build.py",
                    "-a",
                    "x86_64",
                    "-b",
                    "1700000000",
                    "-c",
                    "stable",
                    "-d",
                    "chromium",
                    "-o",
                    str(out_dir),
                    "-t",
                    "linux",
                    "--use-static-angle=true",
                ]
                if official:
                    argv.append("-f")

                with mock.patch.object(sys, "argv", argv):
                    build.main()

                rpm_path = (
                    out_dir
                    / "chromium-browser-stable-160.0.8155.0-1.x86_64.rpm"
                )
                self.assertTrue(rpm_path.is_file())

                data = rpm_path.read_bytes()
                # RPM v4 packages use lead major 3 (RPM v6 uses lead major 4).
                self.assertEqual(data[:4], b"\xed\xab\xee\xdb")
                self.assertEqual(data[4], 3)

                # Verify standard SVR4 newc cpio magic (070701) rather than
                # RPM v6 stripped cpio magic (07070X), which breaks libarchive.
                if official:
                    offset = data.find(b"\xfd7zXZ\x00")
                    self.assertGreater(offset, 0)
                    payload_head = lzma.decompress(data[offset:])[:6]
                else:
                    offset = data.find(b"\x1f\x8b\x08")
                    self.assertGreater(offset, 0)
                    with gzip.GzipFile(fileobj=io.BytesIO(data[offset:])) as gz:
                        payload_head = gz.read(6)
                self.assertEqual(payload_head, b"070701")


if __name__ == "__main__":
    unittest.main()
