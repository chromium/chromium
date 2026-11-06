#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
import pathlib
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # pylint: disable=wrong-import-position


class FilterTest(unittest.TestCase):
    def test_is_excluded(self):
        excludes = ['etc', 'usr/share/doc', 'usr/lib/*/libfoo.so*']
        self.assertTrue(build.is_excluded('etc', excludes))
        self.assertTrue(build.is_excluded('etc/xdg/foo', excludes))
        self.assertTrue(
            build.is_excluded('usr/share/doc/a/copyright', excludes)
        )
        self.assertTrue(
            build.is_excluded('usr/lib/x86_64-linux-gnu/libfoo.so.1', excludes)
        )
        self.assertFalse(build.is_excluded('usr/share/docs', excludes))
        self.assertFalse(build.is_excluded('usr/lib/libbar.so', excludes))

    def test_apply_cleanup(self):
        with tempfile.TemporaryDirectory() as tmp:
            prime = pathlib.Path(tmp)
            (prime / 'usr/lib/dri').mkdir(parents=True)
            (prime / 'usr/lib/dri/a_dri.so').write_text('a')
            (prime / 'usr/lib/libx.so.1').write_text('x')
            (prime / 'usr/lib/libx.so').symlink_to('libx.so.1')
            (prime / 'usr/lib/libabs.so').symlink_to('/usr/lib/libabs.so.1')
            (prime / 'usr/lib/liby.so.1').write_text('y')
            build.apply_cleanup(
                prime, ['usr/lib/dri/a_dri.so', 'usr/lib/libx.so.1']
            )
            self.assertFalse((prime / 'usr/lib/dri').exists())
            self.assertFalse(os.path.lexists(prime / 'usr/lib/libx.so'))
            self.assertTrue(os.path.lexists(prime / 'usr/lib/libabs.so'))
            self.assertTrue((prime / 'usr/lib/liby.so.1').exists())

    def test_normalize_and_filter(self):
        with tempfile.TemporaryDirectory() as tmp:
            tree = pathlib.Path(tmp)
            (tree / 'etc').mkdir()
            (tree / 'etc/conf').write_text('x')
            (tree / 'usr/lib').mkdir(parents=True)
            (tree / 'usr/lib/libz.so.1').write_text('z')
            (tree / 'usr/lib/libz.so').symlink_to('/usr/lib/libz.so.1')
            (tree / 'usr/lib/dangling').symlink_to('/nonexistent')
            build.normalize_and_filter(tree, ['-etc'])
            self.assertFalse((tree / 'etc').exists())
            self.assertEqual(os.readlink(tree / 'usr/lib/libz.so'), 'libz.so.1')
            self.assertEqual(
                os.readlink(tree / 'usr/lib/dangling'), '/nonexistent'
            )


class DebTest(unittest.TestCase):
    def test_extract(self):
        tar_buf = io.BytesIO()
        with tarfile.open(fileobj=tar_buf, mode='w:xz') as tar:
            data = b'hello'
            info = tarfile.TarInfo('./usr/bin/hello')
            info.size = len(data)
            info.mode = 0o4755
            tar.addfile(info, io.BytesIO(data))
        members = [
            (b'debian-binary', b'2.0\n'),
            (b'data.tar.xz', tar_buf.getvalue()),
        ]
        deb = io.BytesIO()
        deb.write(b'!<arch>\n')
        for name, content in members:
            header = (
                (name + b'/').ljust(16)
                + b'0'.ljust(12)
                + b'0'.ljust(6)
                + b'0'.ljust(6)
                + b'100644'.ljust(8)
                + str(len(content)).encode().ljust(10)
                + b'`\n'
            )
            deb.write(header + content + (b'\n' if len(content) % 2 else b''))
        with tempfile.TemporaryDirectory() as tmp:
            deb_path = pathlib.Path(tmp, 'x.deb')
            deb_path.write_bytes(deb.getvalue())
            out = pathlib.Path(tmp, 'out')
            build.extract_deb(deb_path, out)
            hello = out / 'usr/bin/hello'
            self.assertEqual(hello.read_bytes(), b'hello')
            self.assertFalse(hello.stat().st_mode & 0o4000)

    def test_check_deb(self):
        with tempfile.TemporaryDirectory() as tmp:
            deb = pathlib.Path(tmp, 'x.deb')
            deb.write_bytes(b'deb')
            pkg = {'size': 3, 'sha256': build.sha256_file(deb)}
            build.check_deb(deb, pkg)
            with self.assertRaises(RuntimeError):
                build.check_deb(deb, dict(pkg, size=4))
            with self.assertRaises(RuntimeError):
                build.check_deb(deb, dict(pkg, sha256='0' * 64))


if __name__ == '__main__':
    unittest.main()
