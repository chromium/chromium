#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import makecab

from StringIO import StringIO
import sys
import time
import unittest

# Test flag parsing.
class ParseFlagsTest(unittest.TestCase):
  def testInputOnly(self):
    flags = makecab.ParseFlags(['/V1', '/D', 'CompressionType=LZX', 'foo.txt'])
    self.assertEquals(flags.input, 'foo.txt')
    self.assertEquals(flags.output, 'foo.tx_')
    self.assertEquals(flags.output_dir, '.')

  def testInputOutput(self):
    flags = makecab.ParseFlags(['bar.txt', 'd/foo.cab'])
    self.assertEquals(flags.input, 'bar.txt')
    self.assertEquals(flags.output, 'd/foo.cab')
    self.assertEquals(flags.output_dir, '.')

  def testInputOutdir(self):
    flags = makecab.ParseFlags(['/L', 'outdir', 'baz.txt'])
    self.assertEquals(flags.input, 'baz.txt')
    self.assertEquals(flags.output, 'baz.tx_')
    self.assertEquals(flags.output_dir, 'outdir')

  def testInputOutputOutdir(self):
    flags = makecab.ParseFlags(['/L', 'outdir', 'foo.txt', 'd/foo.cab'])
    self.assertEquals(flags.input, 'foo.txt')
    self.assertEquals(flags.output, 'd/foo.cab')
    self.assertEquals(flags.output_dir, 'outdir')

  def testHelp(self):
    self.assertEquals(makecab.ParseFlags(['foo.txt', '--help']), None)

  def assertFlagParseError(self, flags, expected_message_part):
    with self.assertRaises(makecab.FlagParseError) as context:
      makecab.ParseFlags(flags)
    self.assertIn(expected_message_part, context.exception.message)

  def testErrors(self):
    for f in ['/D', '/L']:
      self.assertFlagParseError([f], 'argument needed after')
    self.assertFlagParseError(['/asdf'], 'unknown flag')
    self.assertFlagParseError(['in', 'out', 'what'], 'too many paths')
    self.assertFlagParseError([], 'no input file')

# Test that compression doesn't throw, and on Windows also check that
# expand.exe is able to recover input data.
class WriteCabTest(unittest.TestCase):
  def testWellCompressingInput(self):
    input_data = 'a' * (4 << 15)
    output = StringIO()
    mtime = time.mktime((2018, 1, 8,  16, 00, 00,  0, 8, -1))
    makecab.WriteCab(output, StringIO(input_data), 'a.txt',
                     len(input_data), mtime)
    if sys.platform == 'win32':
      import os, shutil, subprocess, tempfile
      temp_dir = tempfile.mkdtemp(suffix='.makecab_test')
      try:
        cab_path = os.path.join(temp_dir, 'file.cab')
        out_path = os.path.join(temp_dir, 'file.out')
        open(cab_path, 'wb').write(output.getvalue())
        FNULL = open(os.devnull, 'w')
        subprocess.check_call(['expand.exe', cab_path, out_path], stdout=FNULL)
        self.assertEquals(input_data, open(out_path, 'rb').read())
      finally:
        shutil.rmtree(temp_dir)


if __name__ == '__main__':
  unittest.main()
