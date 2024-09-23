# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import shutil
import subprocess
import sys
import unittest

from signing import commands


class TestCommands(unittest.TestCase):

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tempdir)

    def test_file_exists(self):
        path = os.path.join(self.tempdir, 'exists.txt')

        self.assertFalse(commands.file_exists(path))

        open(path, 'w').close()
        self.assertTrue(commands.file_exists(path))

        os.unlink(path)
        self.assertFalse(commands.file_exists(path))

    def test_delete_file_if_exists(self):
        file_path = os.path.join(self.tempdir, 'file.txt')

        commands.write_file(file_path, 'moo')
        self.assertTrue(commands.file_exists(file_path))

        commands.delete_file_if_exists(file_path)
        self.assertFalse(commands.file_exists(file_path))

        # Execute it one more time, just to make sure the exception
        # is ignored in the event the file does not exist.
        commands.delete_file_if_exists(file_path)

    def test_copy_dir_overwrite_and_count_changes(self):
        source_dir = os.path.join(self.tempdir, 'source')
        os.mkdir(source_dir)

        os.mkdir(os.path.join(source_dir, 'dir'))
        open(os.path.join(source_dir, 'dir', 'file'), 'w').close()
        with open(os.path.join(source_dir, 'file'), 'w') as file:
            file.write('contents')

        dest_dir = os.path.join(self.tempdir, 'dest')

        # Make sure that dry_run doesn't actually change anything by testing it
        # a couple of times before doing any real work.
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=True), 4)
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=True), 4)
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 4)

        # Now test that a subsequent copy of the same thing doesn't report any
        # changes.
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=True), 0)
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 0)

        self.assertTrue(os.path.isdir(dest_dir))
        self.assertTrue(os.path.isfile(os.path.join(dest_dir, 'file')))
        self.assertTrue(os.path.isdir(os.path.join(dest_dir, 'dir')))
        self.assertTrue(os.path.isfile(os.path.join(dest_dir, 'dir', 'file')))

        self.assertEqual(
            os.path.getsize(os.path.join(dest_dir, 'dir', 'file')), 0)
        with open(os.path.join(dest_dir, 'file')) as file:
            self.assertEqual(file.read(), 'contents')

        # No changes to source should result in no changes reported.
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 0)

        # Changing a timestamp isn't reported, but the timestamp does get
        # updated.
        os.utime(os.path.join(source_dir, 'dir', 'file'), (0, 0))

        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 0)

        self.assertEqual(
            os.path.getmtime(os.path.join(dest_dir, 'dir', 'file')), 0)

        # Changing a file is reported.
        with open(os.path.join(source_dir, 'file'), 'w') as file:
            file.write('new contents')
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 1)

        with open(os.path.join(dest_dir, 'file')) as file:
            self.assertEqual(file.read(), 'new contents')

        # Changing a file whose length doesn't change is reported.
        with open(os.path.join(source_dir, 'file'), 'w') as file:
            file.write('new_contents')
        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 1)

        with open(os.path.join(dest_dir, 'file')) as file:
            self.assertEqual(file.read(), 'new_contents')

        # Creating directories and files are reported.
        os.mkdir(os.path.join(source_dir, 'new_dir'))
        open(os.path.join(source_dir, 'new_file'), 'w').close()

        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 2)

        self.assertTrue(os.path.isfile(os.path.join(dest_dir, 'new_file')))
        self.assertTrue(os.path.isdir(os.path.join(dest_dir, 'new_dir')))

        # Removing files and directories is also reported.
        os.rmdir(os.path.join(source_dir, 'new_dir'))
        os.unlink(os.path.join(source_dir, 'new_file'))
        os.mkdir(os.path.join(source_dir, 'newer_dir'))
        open(os.path.join(source_dir, 'newer_file'), 'w').close()

        self.assertEqual(
            commands.copy_dir_overwrite_and_count_changes(
                source_dir, dest_dir, dry_run=False), 4)

        self.assertFalse(os.path.exists(os.path.join(dest_dir, 'new_file')))
        self.assertFalse(os.path.exists(os.path.join(dest_dir, 'new_dir')))
        self.assertTrue(os.path.isfile(os.path.join(dest_dir, 'newer_file')))
        self.assertTrue(os.path.isdir(os.path.join(dest_dir, 'newer_dir')))

    def test_move_file(self):
        orig_path = os.path.join(self.tempdir, 'file.txt')
        new_path = os.path.join(self.tempdir, 'renamed.txt')

        commands.write_file(orig_path, 'moo')

        self.assertTrue(commands.file_exists(orig_path))
        self.assertFalse(commands.file_exists(new_path))

        commands.move_file(orig_path, new_path)

        self.assertFalse(commands.file_exists(orig_path))
        self.assertTrue(commands.file_exists(new_path))

    def test_read_write_file(self):
        path = os.path.join(self.tempdir, 'file.txt')

        data1 = 'hello world this is a test'
        commands.write_file(path, data1)
        data1_read = commands.read_file(path)
        self.assertEqual(data1, data1_read)

        data2 = 'moo'
        commands.write_file(path, data2)
        data2_read = commands.read_file(path)
        self.assertEqual(data2, data2_read)

    def test_zip(self):
        content_path = os.path.join(self.tempdir, 'zipfile', 'file.txt')
        output_path = os.path.join(self.tempdir, 'out.zip')
        os.mkdir(os.path.dirname(content_path))
        commands.write_file(content_path, 'moon')
        self.assertFalse(commands.file_exists(output_path))
        commands.zip(output_path, os.path.dirname(content_path))
        self.assertTrue(commands.file_exists(output_path))

    def test_run_command(self):
        path = os.path.join(self.tempdir, 'touch.txt')
        self.assertFalse(commands.file_exists(path))

        commands.run_command(['touch', path])

        self.assertTrue(commands.file_exists(path))

    def test_run_command_with_default_stderr(self):
        r, w = os.pipe()
        try:
            commands.run_command([
                sys.executable, '-c',
                'import sys; sys.stdout.write("Out."); sys.stdout.flush(); sys.stderr.write("Error."); sys.exit(33)'
            ],
                                 stdout=w)
            self.fail('Should have thrown')
        except subprocess.CalledProcessError as e:
            os.close(w)
            self.assertEqual(33, e.returncode)
            self.assertEqual(b'Out.', os.read(r, 128))
        os.close(r)

    def test_run_command_with_stderr(self):
        ro, wo = os.pipe()
        re, we = os.pipe()
        try:
            commands.run_command([
                sys.executable, '-c',
                'import sys; sys.stdout.write("Out."); sys.stderr.write("Error."); sys.exit(19)'
            ],
                                 stdout=wo,
                                 stderr=we)
            self.fail('Should have thrown')
        except subprocess.CalledProcessError as e:
            os.close(wo)
            os.close(we)
            self.assertEqual(19, e.returncode)
            self.assertEqual(b'Out.', os.read(ro, 128))
            self.assertEqual(b'Error.', os.read(re, 128))
        os.close(ro)
        os.close(re)

    def test_run_command_output(self):
        output = commands.run_command_output(['echo', 'hello world'])
        self.assertEqual(b'hello world\n', output)

    def test_run_command_output_with_default_stderr(self):
        try:
            commands.run_command_output([
                sys.executable, '-c',
                'import sys; sys.stdout.write("Out."); sys.stdout.flush(); sys.stderr.write("Error."); sys.exit(10)'
            ])
            self.fail('Should have thrown')
        except subprocess.CalledProcessError as e:
            self.assertEqual(10, e.returncode)
            self.assertEqual(b'Out.', e.output)

    def test_run_command_output_with_stderr(self):
        r, w = os.pipe()
        try:
            commands.run_command_output([
                sys.executable, '-c',
                'import sys; sys.stdout.write("Out."); sys.stderr.write("Error."); sys.exit(5)'
            ],
                                        stderr=w)
            self.fail('Should have thrown')
        except subprocess.CalledProcessError as e:
            os.close(w)
            self.assertEqual(5, e.returncode)
            self.assertEqual(b'Out.', e.output)
            self.assertEqual(b'Error.', os.read(r, 128))
        os.close(r)

    def test_lenient_run_command_output(self):
        # Successful command, output on stdout.
        (returncode, stdout,
         stderr) = commands.lenient_run_command_output(['echo', 'hello'])
        self.assertEqual(returncode, 0)
        self.assertEqual(stdout, b'hello\n')
        self.assertEqual(stderr, b'')

        # Failure, error on stderr.
        (returncode, stdout,
         stderr) = commands.lenient_run_command_output(['cp'])
        self.assertNotEqual(returncode, 0)
        self.assertEqual(stdout, b'')
        self.assertTrue(b'usage: ' in stderr or b'cp: ' in stderr)

        # EACCES
        (returncode, stdout,
         stderr) = commands.lenient_run_command_output(['/etc/shells'])
        self.assertIsNone(returncode)
        self.assertIsNone(stdout)
        self.assertIsNone(stderr)

        # ENOENT
        (returncode, stdout,
         stderr) = commands.lenient_run_command_output(['/var/empty/enoent'])
        self.assertIsNone(returncode)
        self.assertIsNone(stdout)
        self.assertIsNone(stderr)

    def test_macos_version(self):
        version = commands.macos_version()
        self.assertGreaterEqual(len(version), 2)
        self.assertGreaterEqual(version, [10, 10])
        self.assertLess(version, [30])

    def test_plist_context_xml(self):
        path = os.path.join(self.tempdir, 'plist.strings')
        with commands.PlistContext(
                path, rewrite=True, create_new=True) as plist:
            plist['A'] = 'B'
            plist['C'] = 'D'

        # Verify that the file is an XML file.
        with open(path, 'rb') as file:
            self.assertEqual(file.read(5), b'<?xml')

        data = commands.read_plist(path)
        self.assertEqual(data, {'A': 'B', 'C': 'D'})

    def test_plist_context_binary(self):
        path = os.path.join(self.tempdir, 'plist.strings')
        with commands.PlistContext(
                path, rewrite=True, create_new=True, binary=True) as plist:
            plist['A'] = 'B'
            plist['C'] = 'D'

        # Verify that the file is a binary Plist file.
        with open(path, 'rb') as file:
            self.assertEqual(file.read(8), b'bplist00')

        data = commands.read_plist(path)
        self.assertEqual(data, {'A': 'B', 'C': 'D'})
