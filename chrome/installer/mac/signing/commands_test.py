# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import shutil
import unittest

from . import commands


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

    def test_write_file(self):
        path = os.path.join(self.tempdir, 'file.txt')

        data1 = 'hello world this is a test'
        commands.write_file(path, data1)

        with open(path, 'r') as f:
            self.assertEqual(data1, f.read())

        data2 = 'moo'
        commands.write_file(path, data2)

        with open(path, 'r') as f:
            self.assertEqual(data2, f.read())

    def test_run_command(self):
        path = os.path.join(self.tempdir, 'touch.txt')
        self.assertFalse(commands.file_exists(path))

        commands.run_command(['touch', path])

        self.assertTrue(commands.file_exists(path))

    def test_run_command_output(self):
        output = commands.run_command_output(['echo', 'hello world'])
        self.assertEqual(b'hello world\n', output)
