#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import update  # pylint: disable=wrong-import-position


class ParseTest(unittest.TestCase):
    def test_control(self):
        stanzas = update.parse_control(
            'Package: a\nDescription: x\n more\n\nPackage: b\nVersion: 1\n'
        )
        self.assertEqual(
            stanzas,
            [
                {'Package': 'a', 'Description': 'x\nmore'},
                {'Package': 'b', 'Version': '1'},
            ],
        )

    def test_base_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp, 'dpkg.yaml')
            path.write_text(
                'packages:\n- libc6=2.43-2ubuntu2\n- libgcc-s1:amd64=16-1\n'
            )
            self.assertEqual(
                update.parse_base_manifest(path),
                {'libc6': '2.43-2ubuntu2', 'libgcc-s1': '16-1'},
            )

    def test_inst_line(self):
        self.assertEqual(
            update.INST_RE.match(
                'Inst libnss3 (2:3.117-1ubuntu1 Ubuntu:26.04/resolute [amd64])'
            ).groups(),
            ('libnss3', '2:3.117-1ubuntu1', 'amd64'),
        )
        self.assertEqual(
            update.INST_RE.match(
                'Inst libc6 [2.43-2ubuntu2] (2.43-2ubuntu2.4 '
                'Ubuntu:26.04/resolute-updates [amd64])'
            ).groups(),
            ('libc6', '2.43-2ubuntu2.4', 'amd64'),
        )
        self.assertIsNone(update.INST_RE.match('Conf libc6 (2.43 [amd64])'))


class AptTest(unittest.TestCase):
    def test_private_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            apt = update.Apt(
                tmp,
                'https://example.com/ubuntu',
                ['resolute', 'resolute-updates'],
                ['main'],
            )
            config = pathlib.Path(tmp, 'apt.conf').read_text()
            self.assertIn(f'Dir "{tmp}/";', config)
            self.assertIn('APT::Install-Recommends "false";', config)
            sources = pathlib.Path(
                tmp, 'etc/apt/sources.list.d/ubuntu.sources'
            ).read_text()
            self.assertIn('Suites: resolute resolute-updates\n', sources)
            self.assertIn(f'Signed-By: {update.KEYRING_FILE}\n', sources)
            self.assertEqual(
                apt.archive_path('libnss3', '2:3.117-1', 'amd64').name,
                'libnss3_2%3a3.117-1_amd64.deb',
            )


class CleanupTest(unittest.TestCase):
    def test_matches_glob(self):
        self.assertTrue(
            update.matches_glob(
                'usr/lib/x86_64-linux-gnu/dri/iris_dri.so', 'usr/lib/*/dri/*'
            )
        )
        self.assertTrue(
            update.matches_glob(
                'usr/share/X11/locale/C/XLC_LOCALE', 'usr/share/X11/locale/*'
            )
        )
        self.assertFalse(
            update.matches_glob('usr/lib/dri/iris_dri.so', 'usr/lib/*/dri/*')
        )
        self.assertFalse(update.matches_glob('usr/lib', 'usr/lib/*/dri/*'))

    def test_is_shared_library(self):
        self.assertTrue(update.is_shared_library('usr/lib/libfoo.so'))
        self.assertTrue(update.is_shared_library('usr/lib/libfoo.so.1.2'))
        self.assertFalse(update.is_shared_library('usr/lib/libfoo.a'))
        self.assertFalse(update.is_shared_library('usr/lib/libfoo.sox'))

    def test_next_version(self):
        self.assertEqual(
            update.next_version('20261103T000000Z', None), '20261103.1'
        )
        self.assertEqual(
            update.next_version('20261103T120000Z', {'version': '20261103.1'}),
            '20261103.2',
        )
        self.assertEqual(
            update.next_version('20261104T000000Z', {'version': '20261103.2'}),
            '20261104.1',
        )


if __name__ == '__main__':
    unittest.main()
