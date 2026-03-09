#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import struct
import subprocess
import tempfile
import unittest

from signing import rebranding


def find_tool(name: str) -> pathlib.Path | None:
    cwd_tool = pathlib.Path.cwd() / name
    if cwd_tool.exists():
        return cwd_tool

    # Heuristic for running directly not via isolated_script_test
    src_root = pathlib.Path(__file__).resolve().parents[4]
    for out_dir in ['out/Debug', 'out/Release', 'out/Default']:
        tool = src_root / out_dir / name
        if tool.exists():
            return tool

    return None


class TestRebranding(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory(prefix="rebrand_test_")
        self.temp_path = pathlib.Path(self.temp_dir.name)

        self.hfs_tool_path = find_tool("hfs_tool")
        self.dmg_tool_path = find_tool("dmg_tool")

        self.assertIsNotNone(self.hfs_tool_path)
        self.assertIsNotNone(self.dmg_tool_path)

        self.hfs_tool = rebranding.HfsTool(self.hfs_tool_path)
        self.dmg_tool = rebranding.DmgTool(self.dmg_tool_path)
        self.hdiutil = rebranding.HdiUtil()

    def tearDown(self):
        self.temp_dir.cleanup()

    def _create_example_app(self, path: pathlib.Path, name: str):
        app_path = path / name
        app_path.mkdir(parents=True)
        (app_path / "Contents").mkdir()
        (app_path / "Contents" / "Info.plist").write_text("Example Info.plist")
        (app_path / "Contents" / "MacOS").mkdir()
        (app_path / "Contents" / "MacOS" / name).write_text("Example Binary")

    def _create_example_dmg(self, output_path: pathlib.Path,
                            src_dir: pathlib.Path, volname: str):
        subprocess.check_call([
            "hdiutil",
            "create",
            "-srcfolder",
            str(src_dir),
            "-volname",
            volname,
            "-format",
            "UDRW",  # Read/Write for simplicity in creation
            str(output_path)
        ])

    def test_rebrand_chromium_dmg(self):
        src_root = self.temp_path / "src"
        src_root.mkdir()
        self._create_example_app(src_root, "Google Chrome.app")

        source_dmg = self.temp_path / "source.dmg"
        self._create_example_dmg(source_dmg, src_root, "Google Chrome")

        output_dir = self.temp_path / "output"
        brand_codes = ["TEST", "ABCD"]

        rebranding.rebrand_chromium_dmg(self.hfs_tool, self.dmg_tool,
                                        source_dmg, output_dir, brand_codes,
                                        "Google Chrome", "123.0.0.0", "Stable")

        for brand in brand_codes:
            expected_output = (
                output_dir / f"GoogleChrome-123.0.0.0-Stable-Brand-{brand}.dmg")
            self.assertTrue(expected_output.exists())

            expected_tag = f"brand={brand}".encode("ascii")

            with self.hdiutil.scoped_attach(expected_output) as mount_path:
                app_path = mount_path / "Google Chrome.app"
                self.assertTrue(app_path.exists())

                info_plist = app_path / "Contents" / "Info.plist"
                self.assertTrue(info_plist.exists())
                self.assertEqual(info_plist.read_text(), "Example Info.plist")

                macos_bin = (
                    app_path / "Contents" / "MacOS" / "Google Chrome.app")
                self.assertTrue(macos_bin.exists())
                self.assertEqual(macos_bin.read_text(), "Example Binary")

                raw_output = subprocess.check_output([
                    "xattr", "-p", "-x", "com.apple.application-instance",
                    app_path
                ],
                                                     stderr=subprocess.PIPE)
                output = bytes.fromhex(raw_output.decode("ascii"))

                magic = b"Gact2.0Omaha"
                self.assertTrue(output.startswith(magic))

                tag_len = struct.unpack(">H", output[12:14])[0]
                tag_content = output[14:14 + tag_len]
                self.assertEqual(tag_content, expected_tag)

            # 3. Verify auto-open folder is set to root (ID 2)
            fsid_output = subprocess.check_output(
                ["hdiutil", "fsid", expected_output]).decode("utf-8")
            # Look for the "finderInfo" section of `hdiutil fsid`'s output.
            found_open_folder = 0
            in_finder_info = False
            in_correct_partition = False
            for line in fsid_output.splitlines():
                if line.startswith("Analyzing partition "):
                    in_correct_partition = line.endswith("Mac_OS_X Apple_HFSX")
                if not in_correct_partition:
                    continue
                if line.strip() == "finderInfo":
                    in_finder_info = True
                    continue
                if in_finder_info:
                    parts = line.split()
                    if len(parts) < 2:
                        # Exited the FinderInfo segment.
                        in_finder_info = False
                        continue
                    if parts[0] == "2":
                        found_open_folder += 1
                        self.assertEqual(
                            parts[1], "2",
                            f"finderInfo[2] should be 2, got {parts[1]} "
                            f"(instance {found_open_folder})")
                        # This is the only thing we wanted in FinderInfo.
                        in_finder_info = False
                        continue
            self.assertEqual(
                found_open_folder, 2,
                "Wrong number of finderInfo[2] records, expected one each for "
                "volume header and alternate volume header")


if __name__ == '__main__':
    unittest.main()
