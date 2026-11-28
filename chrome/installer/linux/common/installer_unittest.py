#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import configparser
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT_DIR = pathlib.Path(__file__).parent.absolute()
LINUX_INSTALLER_DIR = SCRIPT_DIR.parent
REPO_ROOT = LINUX_INSTALLER_DIR.parents[2]
sys.path.insert(0, str(SCRIPT_DIR))

import installer  # noqa: E402


def _read_desktop_entry(path: pathlib.Path) -> dict[str, str]:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    parser.read(path, encoding="utf-8")
    return dict(parser["Desktop Entry"])


class RemoveDesktopEntryKeysTest(unittest.TestCase):
    def test_removes_keys_only_from_desktop_entry_group(self):
        content = (
            "[Desktop Entry]\n"
            "Name=Chromium\n"
            "MimeType=text/html;\n"
            "StartupWMClass=chromium\n"
            "StartupWMClassic=kept\n"
            "Comment[de]=Internetzugriff\n"
            "\n"
            "[Desktop Action new-window]\n"
            "MimeType=kept\n"
        )
        self.assertEqual(
            installer.remove_desktop_entry_keys(
                content, ("MimeType", "StartupWMClass", "Comment")
            ),
            "[Desktop Entry]\n"
            "Name=Chromium\n"
            "StartupWMClassic=kept\n"
            "\n"
            "[Desktop Action new-window]\n"
            "MimeType=kept\n",
        )


# The packaging steps need Linux tools (gzip) and symlinks, and the
# installer only runs on Linux build hosts.
@unittest.skipUnless(
    sys.platform.startswith("linux"), "Linux packaging tools required"
)
class DesktopIntegrationTest(unittest.TestCase):
    def _stage_output_dir(self, out_dir: pathlib.Path) -> None:
        common_out = out_dir / "installer/common"
        theme_out = out_dir / "installer/theme"
        common_out.mkdir(parents=True)
        theme_out.mkdir(parents=True)
        for src in SCRIPT_DIR.iterdir():
            if src.is_file() and not src.name.endswith(".py"):
                shutil.copy(src, common_out / src.name)
        shutil.copy(
            REPO_ROOT / "chrome/app/resources/manpage.1.in",
            common_out / "manpage.1.in",
        )
        (theme_out / "BRANDING").write_text(
            "COMPANY_FULLNAME=The Chromium Authors\n", encoding="utf-8"
        )
        (out_dir / "installer/version.txt").write_text(
            "MAJOR=160\nMINOR=0\nBUILD=8155\nPATCH=0\n", encoding="utf-8"
        )

    def _make_config(
        self, out_dir: pathlib.Path, channel: str
    ) -> installer.InstallerConfig:
        args = installer.parse_common_args().parse_args(
            [
                "-a",
                "x86_64",
                "-b",
                "1700000000",
                "-c",
                channel,
                "-d",
                "chromium",
                "-o",
                str(out_dir),
                "-t",
                "linux",
            ]
        )
        config = installer.InstallerConfig.from_args(
            args, out_dir, package_format=installer.PackageFormat.RPM
        )
        config.staging_dir = out_dir / "staging"
        config.tmp_file_dir = out_dir
        return config

    def test_rdn_desktop_file_has_no_handlers(self):
        for channel in ("stable", "unstable"):
            with (
                self.subTest(channel=channel),
                tempfile.TemporaryDirectory() as tmpdir,
            ):
                out_dir = pathlib.Path(tmpdir)
                self._stage_output_dir(out_dir)
                config = self._make_config(out_dir, channel)
                install_dir = out_dir / "install"
                install_dir.mkdir()
                installer.Installer(config)._process_artifacts(
                    config.get_desktop_integration_artifacts(), install_dir
                )

                apps_dir = config.staging_dir / "usr/share/applications"
                package = config.info_vars["PACKAGE"]
                primary = _read_desktop_entry(apps_dir / f"{package}.desktop")
                rdn = _read_desktop_entry(
                    apps_dir / f"{config.rdn_desktop}.desktop"
                )

                self.assertIn("x-scheme-handler/https;", primary["MimeType"])
                self.assertEqual(primary["StartupWMClass"], package)
                self.assertIsNone(primary.get("NoDisplay"))

                self.assertIsNone(rdn.get("MimeType"))
                self.assertIsNone(rdn.get("StartupWMClass"))
                self.assertEqual(rdn["NoDisplay"], "true")
                self.assertEqual(rdn["Exec"], primary["Exec"])
                self.assertEqual(rdn["Icon"], primary["Icon"])
                self.assertEqual(rdn["Name"], primary["Name"])


@unittest.skipUnless(
    sys.platform.startswith("linux"), "Linux wrapper script test"
)
class WrapperTest(unittest.TestCase):
    def test_execs_binary_without_wrapper_argv0(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = pathlib.Path(tmpdir).resolve()
            wrapper_src = (SCRIPT_DIR / "wrapper").read_text(encoding="utf-8")
            wrapper_path = tmp / "google-chrome"
            wrapper_path.write_text(
                wrapper_src.replace("@@channel", "stable").replace(
                    "@@PROGNAME", "chrome"
                ),
                encoding="utf-8",
            )
            wrapper_path.chmod(0o755)

            # Symlink an ELF binary as `chrome` so argv[0] is preserved
            # verbatim (a `#!` script would replace argv[0] with the path).
            binary_path = tmp / "chrome"
            binary_path.symlink_to(shutil.which("bash"))

            result_file = tmp / "result.txt"
            stdio_file = tmp / "stdio.txt"
            env = os.environ.copy()
            env["XDG_DATA_HOME"] = str(tmp / "xdg")
            env["RESULT_FILE"] = str(result_file)

            with stdio_file.open("wb") as stdio:
                subprocess.run(
                    [
                        str(wrapper_path),
                        "-c",
                        'printf "%s\\n%s\\n%s\\n%s\\n" '
                        '"$0" '
                        '"$CHROME_WRAPPER" '
                        '"$(test -p /dev/stdout && echo pipe || echo other)" '
                        '"$(test -p /dev/stderr && echo pipe || echo other)" '
                        '> "$RESULT_FILE"',
                    ],
                    env=env,
                    stdout=stdio,
                    stderr=stdio,
                    check=True,
                )

            argv0, chrome_wrapper, stdout_kind, stderr_kind = (
                result_file.read_text(encoding="utf-8").splitlines()
            )
            self.assertEqual(argv0, str(binary_path))
            self.assertEqual(chrome_wrapper, str(wrapper_path))
            self.assertEqual(stdout_kind, "pipe")
            self.assertEqual(stderr_kind, "pipe")


if __name__ == "__main__":
    unittest.main()
