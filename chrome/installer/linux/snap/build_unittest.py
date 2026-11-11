#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import unittest

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
LINUX_INSTALLER_DIR = SCRIPT_DIR.parent
REPO_ROOT = LINUX_INSTALLER_DIR.parents[2]
sys.path.insert(0, str(SCRIPT_DIR))
sys.path.insert(0, str(LINUX_INSTALLER_DIR / "common"))

import build as snap_build  # noqa: E402
import installer  # noqa: E402


def setup_fake_output_dir(
    output_dir: pathlib.Path, branding: str = "chromium"
) -> None:
    common_dir = output_dir / "installer/common"
    theme_dir = output_dir / "installer/theme"
    common_dir.mkdir(parents=True, exist_ok=True)
    theme_dir.mkdir(parents=True, exist_ok=True)

    for name in (
        "chromium-browser.info",
        "google-chrome.info",
        "desktop.template",
        "wrapper",
    ):
        shutil.copy2(LINUX_INSTALLER_DIR / "common" / name, common_dir / name)
    shutil.copy2(
        REPO_ROOT / "chrome/app/resources/manpage.1.in",
        common_dir / "manpage.1.in",
    )

    (theme_dir / "BRANDING").write_text(
        "COMPANY_FULLNAME=The Chromium Authors\n"
        if branding == "chromium"
        else "COMPANY_FULLNAME=Google LLC\n",
        encoding="utf-8",
    )
    (output_dir / "installer/version.txt").write_text(
        "MAJOR=158\nMINOR=0\nBUILD=8127\nPATCH=0\n", encoding="utf-8"
    )


def make_config(
    output_dir: pathlib.Path,
    staging_dir: pathlib.Path,
    branding: str = "chromium",
    channel: str = "stable",
    arch: str = "amd64",
) -> installer.InstallerConfig:
    parser = installer.parse_common_args()
    args = parser.parse_args(
        [
            "-a",
            arch,
            "-b",
            "1700000000",
            "-c",
            channel,
            "-d",
            branding,
            "-o",
            str(output_dir),
            "-t",
            "linux",
        ]
    )
    config = installer.InstallerConfig.from_args(
        args, output_dir, package_format=installer.PackageFormat.SNAP
    )
    config.script_dir = SCRIPT_DIR
    config.staging_dir = staging_dir
    config.tmp_file_dir = output_dir
    config.shlib_perms = installer.StandardPermissions.REGULAR
    return config


class SnapBuildTest(unittest.TestCase):
    def test_stage_chromium_stable_and_cipd_symlink_runtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            out_dir = root / "out"
            staging_dir = root / "staging"
            setup_fake_output_dir(out_dir, branding="chromium")

            config = make_config(
                out_dir, staging_dir, branding="chromium", channel="stable"
            )
            install_dir = staging_dir / config.info_vars["INSTALLDIR"].lstrip(
                "/"
            )
            install_dir.mkdir(parents=True)
            (install_dir / "libqt5_shim.so").write_bytes(b"qt5")
            (install_dir / "libqt6_shim.so").write_bytes(b"qt6")
            sandbox = install_dir / "chrome-sandbox"
            sandbox.write_bytes(b"sandbox")
            sandbox.chmod(0o4755)

            # Simulate a CIPD package deployed in symlink mode (.cipd/pkgs/...).
            cipd_store = root / ".cipd/pkgs/0/inst"
            (cipd_store / "prime/usr/lib/x86_64-linux-gnu").mkdir(parents=True)
            (
                cipd_store / "prime/usr/lib/x86_64-linux-gnu/libfoo.so.1.0"
            ).write_bytes(b"elfdata")
            (
                cipd_store / "prime/usr/lib/x86_64-linux-gnu/libfoo.so.1"
            ).symlink_to("libfoo.so.1.0")
            (cipd_store / "prime/usr/share/X11").mkdir(parents=True)
            (cipd_store / "prime/usr/share/X11/rgb.txt").symlink_to(
                "/etc/X11/rgb.txt"
            )
            (cipd_store / "copyright").mkdir(parents=True)
            (cipd_store / "copyright/libfoo1").write_text(
                "Copyright foo\n", encoding="utf-8"
            )
            (root / ".cipd/pkgs/0/_current").symlink_to("inst")

            runtime_dir = root / "runtime/linux-amd64"
            for rel in (
                "prime/usr/lib/x86_64-linux-gnu/libfoo.so.1.0",
                "prime/usr/lib/x86_64-linux-gnu/libfoo.so.1",
                "prime/usr/share/X11/rgb.txt",
                "copyright/libfoo1",
            ):
                link = runtime_dir / rel
                link.parent.mkdir(parents=True, exist_ok=True)
                target = root / ".cipd/pkgs/0/_current" / rel
                link.symlink_to(os.path.relpath(target, link.parent))

            snap_build.stage_snap_files(
                config, SCRIPT_DIR, out_dir, staging_dir, runtime_dir
            )

            # Qt5 shim removed, Qt6 shim kept, setuid cleared on chrome-sandbox.
            self.assertFalse((install_dir / "libqt5_shim.so").exists())
            self.assertTrue((install_dir / "libqt6_shim.so").is_file())
            self.assertEqual(stat.S_IMODE(sandbox.lstat().st_mode), 0o755)

            # CIPD symlinks unwrapped: real file copied, internal symlink kept.
            lib_real = staging_dir / "usr/lib/x86_64-linux-gnu/libfoo.so.1.0"
            lib_link = staging_dir / "usr/lib/x86_64-linux-gnu/libfoo.so.1"
            self.assertFalse(lib_real.is_symlink())
            self.assertEqual(lib_real.read_bytes(), b"elfdata")
            self.assertTrue(lib_link.is_symlink())
            self.assertEqual(os.readlink(lib_link), "libfoo.so.1.0")
            self.assertFalse(
                os.path.lexists(staging_dir / "usr/share/X11/rgb.txt")
            )
            self.assertEqual(
                (staging_dir / "usr/share/doc/libfoo1/copyright").read_text(
                    encoding="utf-8"
                ),
                "Copyright foo\n",
            )

            # Metadata, desktop entry, hooks, command-chain, launcher, and man.
            snap_yaml = (staging_dir / "meta/snap.yaml").read_text(
                encoding="utf-8"
            )
            self.assertNotIn("@@", snap_yaml)
            self.assertFalse(
                any(
                    line.lstrip().startswith("#")
                    for line in snap_yaml.splitlines()
                )
            )
            self.assertIn("name: chromium\n", snap_yaml)
            self.assertIn("chromium-config:", snap_yaml)
            self.assertIn("- $HOME/.config/chromium\n", snap_yaml)
            self.assertIn("dot-local-share-applications:", snap_yaml)
            self.assertIn("dot-local-share-icons:", snap_yaml)
            self.assertIn(
                "QT_PLUGIN_PATH: $SNAP/usr/lib/x86_64-linux-gnu/qt6/plugins",
                snap_yaml,
            )

            desktop = (staging_dir / "meta/gui/chromium.desktop").read_text(
                encoding="utf-8"
            )
            self.assertNotIn("@@", desktop)
            self.assertIn("Exec=chromium %U\n", desktop)
            self.assertIn("StartupWMClass=chromium\n", desktop)
            self.assertIn(
                "Icon=${SNAP}/opt/chromium.org/chromium/product_logo_256.png\n",
                desktop,
            )

            for exe in (
                "bin/chrome.launcher",
                "bin/man",
                "meta/hooks/configure",
                "snap/command-chain/gpu-2604-wrapper",
                "snap/command-chain/desktop-launch",
                "snap/command-chain/hooks-configure-fonts",
            ):
                p = staging_dir / exe
                self.assertTrue(p.is_file(), exe)
                self.assertEqual(stat.S_IMODE(p.stat().st_mode), 0o755, exe)

            self.assertEqual(
                os.readlink(staging_dir / "usr/bin/xdg-email"),
                "/usr/bin/xdg-open",
            )
            help_txt = (
                staging_dir / "usr/share/doc/chromium-browser/help.txt"
            ).read_text(encoding="utf-8")
            self.assertIn("--user-data-dir", help_txt)
            self.assertNotIn(r"\fB", help_txt)

            for mount_dir in (
                "data-dir/icons",
                "data-dir/sounds",
                "data-dir/themes",
                "gnome-platform/usr/lib/x86_64-linux-gnu/libproxy",
                "gpu-2604",
                "usr/share/alsa",
            ):
                self.assertTrue((staging_dir / mount_dir).is_dir(), mount_dir)

    def test_stage_google_chrome_beta(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            out_dir = root / "out"
            staging_dir = root / "staging"
            runtime_dir = root / "runtime"
            (runtime_dir / "prime").mkdir(parents=True)
            (runtime_dir / "copyright").mkdir()
            setup_fake_output_dir(out_dir, branding="google_chrome")

            config = make_config(
                out_dir, staging_dir, branding="google_chrome", channel="beta"
            )
            (staging_dir / config.info_vars["INSTALLDIR"].lstrip("/")).mkdir(
                parents=True
            )
            snap_build.stage_snap_files(
                config, SCRIPT_DIR, out_dir, staging_dir, runtime_dir
            )

            snap_yaml = (staging_dir / "meta/snap.yaml").read_text(
                encoding="utf-8"
            )
            self.assertIn("name: google-chrome\n", snap_yaml)
            self.assertIn("chrome-config:", snap_yaml)
            self.assertIn("- $HOME/.config/google-chrome-beta\n", snap_yaml)

            desktop = (
                staging_dir / "meta/gui/google-chrome.desktop"
            ).read_text(encoding="utf-8")
            self.assertIn(
                "Icon=${SNAP}/opt/google/chrome-beta/"
                "product_logo_256_beta.png\n",
                desktop,
            )

    @unittest.skipUnless(shutil.which("mksquashfs"), "mksquashfs not installed")
    def test_pack_snap_is_reproducible(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            s1 = root / "s1"
            s2 = root / "s2"
            for s in (s1, s2):
                (s / "meta").mkdir(parents=True)
                (s / "meta/snap.yaml").write_text(
                    "name: test\nversion: '1'\n", encoding="utf-8"
                )
                (s / "link").symlink_to("meta/snap.yaml")

            snap1 = root / "one.snap"
            snap2 = root / "two.snap"
            snap_build.pack_snap(s1, snap1, 1700000000)
            time.sleep(1.1)
            os.utime(s2 / "meta/snap.yaml", None)
            snap_build.pack_snap(s2, snap2, 1700000000)
            self.assertEqual(snap1.read_bytes(), snap2.read_bytes())

            if shutil.which("unsquashfs"):
                listing = subprocess.check_output(
                    ["unsquashfs", "-ll", str(snap1)], text=True
                )
                self.assertIn("root/root", listing)


if __name__ == "__main__":
    unittest.main()
