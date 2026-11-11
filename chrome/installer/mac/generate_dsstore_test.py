# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import generate_dsstore
from ds_store import DSStore
from mac_alias import Alias


class TestGenerateDSStore(unittest.TestCase):
    def test_generate_dsstore(self):
        with tempfile.TemporaryDirectory() as tempdir:
            output_path = os.path.join(tempdir, "chrome_dmg_dsstore")
            generate_dsstore.main(
                [
                    output_path,
                    "--volume-name",
                    "Google Chrome",
                    "--app-name",
                    "Google Chrome.app",
                ]
            )

            self.assertTrue(os.path.isfile(output_path))
            with DSStore.open(output_path, "r") as d:
                self.assertEqual((b"long", 1), d["."]["vSrn"])
                self.assertEqual((b"type", b"icnv"), d["."]["icvl"])
                self.assertEqual(
                    "{{240, 180},{633, 320}}", d["."]["bwsp"]["WindowBounds"]
                )
                self.assertEqual(
                    generate_dsstore.FINDER_BACKGROUND_TYPE_PICTURE,
                    d["."]["icvp"]["backgroundType"],
                )
                alias = Alias.from_bytes(d["."]["icvp"]["backgroundImageAlias"])
                self.assertEqual("Google Chrome", alias.volume.name)
                self.assertEqual(
                    "/.background/background.png", alias.target.posix_path
                )
                self.assertEqual((163, 148), d["Google Chrome.app"]["Iloc"])
                self.assertEqual((470, 148), d[" "]["Iloc"])

    def test_deterministic_output(self):
        with tempfile.TemporaryDirectory() as tempdir:
            out1 = os.path.join(tempdir, "dsstore1")
            out2 = os.path.join(tempdir, "dsstore2")
            args = [
                "--volume-name",
                "Google Chrome Canary",
                "--app-name",
                "Google Chrome Canary.app",
            ]
            generate_dsstore.main([out1, *args])
            generate_dsstore.main([out2, *args])

            with open(out1, "rb") as f1, open(out2, "rb") as f2:
                self.assertEqual(f1.read(), f2.read())
